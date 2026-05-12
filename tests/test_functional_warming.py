"""Tests for the FunctionalWarming phase, plus the SUPPORTS_INTERACTIVE class-level
gate (FW must ignore interactive_tmux because SUPPORTS_INTERACTIVE=False on it)."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
    pin_per_sub,
)


def test_functional_warming_two_nodes(multi_context_for_phase):
    """Vanilla two-node FW: same master-first invariant. The `fw` phase
    overlay in dc-multi.yaml supplies sample_size — driven from YAML, not the
    constructor."""
    from commands.functional_warming import FunctionalWarming
    ctx = multi_context_for_phase("fw")
    with capture_dry_run_stdout() as buf:
        FunctionalWarming(experiment_context=ctx).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "FunctionalWarming", "FunctionalWarming_node")


def test_interactive_tmux_only_on_boot_and_load(multi_context_for_phase):
    """SUPPORTS_INTERACTIVE gate: setting interactive_tmux=True on a context that
    flows into a non-interactive phase (FunctionalWarming) is a no-op — the
    standard subprocess bash is emitted and no [tmux] marker appears."""
    from commands.functional_warming import FunctionalWarming
    top = pin_per_sub(multi_context_for_phase("fw"), interactive_tmux=True)

    with capture_dry_run_stdout() as buf:
        FunctionalWarming(experiment_context=top).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())

    leaves = [b for b in blocks if b.cls_name == "FunctionalWarming"]
    assert len(leaves) == 2
    for b in leaves:
        assert b.tmux_window is None, (
            f"FunctionalWarming should ignore interactive_tmux, but got window {b.tmux_window!r}"
        )
        # Standard subprocess flow: bash is redirected to log/err (the wrapper
        # "( ... ) > log 2> err"), which means _build_bash got a log_path
        # (i.e. the leaf is in subprocess mode, not tmux).
        assert b.bash.rstrip().endswith(".err")
