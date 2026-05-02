"""Tests for the Boot phase: vanilla two-node dispatch, Path A (interaction_script)
on single-node and two-node configs, Path B (interactive_tmux). The interactive
gate (`SUPPORTS_INTERACTIVE`) on non-Boot-Load executors is exercised in
test_functional_warming.py."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
    pin_per_sub,
)


def test_boot_two_nodes(multi_context):
    """Vanilla two-node Boot: master goes first, node 1 waits on master's .started."""
    from commands.boot import Boot
    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Boot", "Boot_node")


def test_boot_with_interaction_script_dry_run(multi_context):
    """Path A: setting interaction_script auto-enables monitor + serial on telnet,
    backgrounds the script, runs QEMU foreground, and waits on the script PID."""
    from commands.boot import Boot
    top = pin_per_sub(multi_context, interaction_script="./drive.exp")

    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=top).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Boot", "Boot_node")

    leaves = [b for b in blocks if b.cls_name == "Boot"]
    assert len(leaves) == 2
    master, follower = leaves[0], leaves[1]

    # Master node 0 → ports 55600 (serial) + 55558 (monitor); env vars present.
    assert "TELNET_SERIAL_PORT=55600" in master.bash
    assert "TELNET_MONITOR_PORT=55558" in master.bash
    assert "NODE_NUMBER=0" in master.bash
    assert "./drive.exp &" in master.bash
    assert "wait $SCRIPT_PID" in master.bash
    assert "-serial telnet:127.0.0.1:55600,server,nowait" in master.bash
    assert "-monitor telnet:127.0.0.1:55558,server,nowait" in master.bash
    assert "-serial mon:stdio" not in master.bash
    assert "-serial file:" not in master.bash

    # Node 1 → ports 55601 + 55559.
    assert "TELNET_SERIAL_PORT=55601" in follower.bash
    assert "TELNET_MONITOR_PORT=55559" in follower.bash
    assert "NODE_NUMBER=1" in follower.bash
    assert "-serial telnet:127.0.0.1:55601,server,nowait" in follower.bash
    assert "-monitor telnet:127.0.0.1:55559,server,nowait" in follower.bash


def test_boot_with_login_ls_script_single_node(mock_mounting_folder, login_ls_script):
    """Path A using sample_scripts/login_and_ls.exp on a SINGLE-NODE config (dc.yaml).

    No multi-node neighbors means is_multi_node() is False, so the auto-port
    machinery doesn't add node_number — telnet ports stay at the bases (55600 / 55558).
    The leaf is the only block in the dry-run output, with no [py-wait] (master
    semantics in the absence of any peer)."""
    from dep_injection.builder import build_experiment_context as b
    e = b("conf/DC/dc.yaml", component_overrides={
        "experiment_context": {
            "mounting_folder": mock_mounting_folder,
            "image_folder": mock_mounting_folder,
            "experiment_name": "single_node_login_ls",
            "interaction_script": login_ls_script,
        }
    })

    from commands.boot import Boot
    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=e).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())

    leaves = [b for b in blocks if b.cls_name == "Boot"]
    assert len(leaves) == 1, f"single-node should produce one leaf, got {len(leaves)}"
    leaf = leaves[0]

    # No multi-node dispatch → no sentinel waits / touches in dry-run output.
    assert leaf.waits == []
    assert leaf.started is None
    assert leaf.done is None

    # Path A bash structure
    assert login_ls_script in leaf.bash, f"script path missing from bash: {leaf.bash}"
    assert "TELNET_SERIAL_PORT=55600" in leaf.bash
    assert "TELNET_MONITOR_PORT=55558" in leaf.bash
    assert "NODE_NUMBER=-1" in leaf.bash    # single-node default
    assert "wait $SCRIPT_PID" in leaf.bash

    # QEMU args use telnet endpoints, not stdio multiplexing.
    assert "-serial telnet:127.0.0.1:55600,server,nowait" in leaf.bash
    assert "-monitor telnet:127.0.0.1:55558,server,nowait" in leaf.bash
    assert "-serial mon:stdio" not in leaf.bash
    assert "-serial file:" not in leaf.bash


def test_boot_with_login_ls_script_two_nodes(multi_context, login_ls_script):
    """Same script, two-node multi config. Master gets serial=55600 / monitor=55558,
    node 1 gets serial=55601 / monitor=55559. Master-first ordering invariant
    holds (node 1 waits on master's Boot_node0.started)."""
    top = pin_per_sub(multi_context, interaction_script=login_ls_script)

    from commands.boot import Boot
    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=top).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Boot", "Boot_node")

    leaves = [b for b in blocks if b.cls_name == "Boot"]
    assert len(leaves) == 2
    master, follower = leaves[0], leaves[1]

    # Master: ports 55600 / 55558, env shows NODE_NUMBER=0.
    assert "TELNET_SERIAL_PORT=55600" in master.bash
    assert "TELNET_MONITOR_PORT=55558" in master.bash
    assert "NODE_NUMBER=0" in master.bash
    assert login_ls_script in master.bash
    assert "-serial telnet:127.0.0.1:55600,server,nowait" in master.bash
    assert "-monitor telnet:127.0.0.1:55558,server,nowait" in master.bash

    # Node 1: ports 55601 / 55559, env shows NODE_NUMBER=1.
    assert "TELNET_SERIAL_PORT=55601" in follower.bash
    assert "TELNET_MONITOR_PORT=55559" in follower.bash
    assert "NODE_NUMBER=1" in follower.bash
    assert login_ls_script in follower.bash
    assert "-serial telnet:127.0.0.1:55601,server,nowait" in follower.bash
    assert "-monitor telnet:127.0.0.1:55559,server,nowait" in follower.bash


def test_boot_interactive_tmux_dry_run(multi_context):
    """Path B: interactive_tmux=True emits a [tmux] marker, leaves the bash with
    mon:stdio (default), appends a `; touch <done>` so Python can poll for QEMU
    exit, and replaces the immediate .done touch with a [py-poll]."""
    from commands.boot import Boot
    top = pin_per_sub(multi_context, interactive_tmux=True)

    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=top).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Boot", "Boot_node")

    leaves = [b for b in blocks if b.cls_name == "Boot"]
    assert len(leaves) == 2
    master, follower = leaves[0], leaves[1]

    # Each leaf gets a uniquely-named tmux window per node.
    assert master.tmux_window == "Boot-node0"
    assert follower.tmux_window == "Boot-node1"

    # Bash uses the default mon:stdio (no telnet auto-flip on Path B), and ends
    # with a `; touch <done-marker>` so the Python poll knows when QEMU exits.
    for b in leaves:
        assert "-serial mon:stdio" in b.bash
        assert "-serial telnet:" not in b.bash
        assert b.bash.rstrip().endswith(b.done)
        assert "; touch " in b.bash

    # The done marker is reported via [py-poll] (parser maps that to .done).
    assert master.done is not None and master.done.endswith("Boot_node0.done")
    assert follower.done is not None and follower.done.endswith("Boot_node1.done")
