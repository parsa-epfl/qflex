"""Tests for RunSinglePartitionCommand — the SequentialGroupExecutor that runs a
fixed partition's idxs in order. Within one node, idx 0 dispatches before idx 1.
Across nodes, master-first holds per (partition, idx) leaf pair."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
)


def test_run_single_partition_two_nodes(multi_context_for_phase):
    """RunSinglePartitionCommand at a fixed partition_number. Each node iterates
    its idxs sequentially via SequentialGroupExecutor; at the per-(partition, idx)
    leaf the master goes first, then the follower. warming_ratio /
    measurement_ratio come from the run_single_partition phase overlay in
    dc-multi.yaml."""
    from commands.run_single_partition import RunSinglePartitionCommand
    from commands.config import clone_experiment_context

    multi_context = multi_context_for_phase("run_single_partition")
    pinned_subs = [
        clone_experiment_context(s, partition_number=0)
        for s in multi_context.sub_experiments
    ]
    top = multi_context.model_copy(update={"sub_experiments": pinned_subs})

    with capture_dry_run_stdout() as buf:
        RunSinglePartitionCommand(experiment_context=top, use_stdio=False).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())

    # Master and follower each emit a RunIdxCommand for idx 0 and idx 1.
    leaves = [b for b in blocks if b.cls_name == "RunIdxCommand"]
    assert len(leaves) == 4

    # Group by (partition, idx) and verify master-first per pair.
    assert_two_node_master_first(blocks, "RunIdxCommand", "RunIdxCommand_part0_")

    # Within a node, idx 0 must dispatch before idx 1 (sequential per-partition).
    master_idx0 = next(b for b in leaves if b.started_basename.endswith("part0_idx0_node0.started"))
    master_idx1 = next(b for b in leaves if b.started_basename.endswith("part0_idx1_node0.started"))
    assert blocks.index(master_idx0) < blocks.index(master_idx1), (
        "within node 0 partition 0, idx 0 should dispatch before idx 1"
    )
