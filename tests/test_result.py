"""Tests for the RunResultCommand (`./qflex result`) phase."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
)


def test_result_two_nodes(multi_context):
    """Result aggregation: per-node leaves with the standard master-first ordering."""
    from commands.result import RunResultCommand
    with capture_dry_run_stdout() as buf:
        RunResultCommand(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "RunResultCommand", "RunResultCommand_node")
