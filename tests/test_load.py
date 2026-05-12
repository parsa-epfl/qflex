"""Tests for the Load phase: vanilla two-node dispatch and Path A
(interaction_script). Path B and the SUPPORTS_INTERACTIVE gate are exercised in
test_boot.py and test_functional_warming.py respectively (Load behaves the same)."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
    pin_per_sub,
)


def test_load_two_nodes(multi_context):
    """Vanilla two-node Load: master goes first, node 1 waits on master's .started."""
    from commands.load import Load
    with capture_dry_run_stdout() as buf:
        Load(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Load", "Load_node")


def test_load_with_interaction_script_dry_run(multi_context):
    """Path A on Load — same shape as Boot's Path A, asserts the dispatch invariant
    plus the script env vars + telnet endpoints in each leaf's bash."""
    from commands.load import Load
    top = pin_per_sub(multi_context, interaction_script="./drive_load.exp")

    with capture_dry_run_stdout() as buf:
        Load(experiment_context=top).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Load", "Load_node")

    leaves = [b for b in blocks if b.cls_name == "Load"]
    for b in leaves:
        assert "./drive_load.exp &" in b.bash
        assert "wait $SCRIPT_PID" in b.bash
        # Path A: serial+monitor go through chardev with logfile, not the
        # bare telnet shorthand.
        assert "-serial chardev:qflex_serial" in b.bash
        assert "-monitor chardev:qflex_monitor" in b.bash
        assert "/qemu_serial.log" in b.bash
        assert "/qemu_monitor.log" in b.bash
        assert "-serial telnet:" not in b.bash
        assert "-monitor telnet:" not in b.bash
        assert "-serial mon:stdio" not in b.bash
