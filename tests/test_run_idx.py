"""Tests for RunIdxCommand — the leaf where per-(partition, idx) sentinel
coordination happens. Sentinel basenames carry both the partition number and
the idx, so two nodes' RunIdxCommand at (partition=P, idx=I) coordinate
master-first with each other (matching the granularity of the per-(part, idx)
shm rings)."""
from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
)


def test_run_idx_at_specific_partition_idx(multi_context):
    """Per-(partition, idx) coordination at the leaf RunIdxCommand level.
    We pin partition_number and idx on each sub-experiment's clone so the sentinel
    namespace is exact: RunIdxCommand_partN_idxM_nodeX."""
    from commands.run_idx import RunIdxCommand
    from commands.config import clone_experiment_context

    # Build a top-level RunIdxCommand pointing at the group context, but each
    # leaf needs partition+idx set. Easiest: clone each sub at (part=0, idx=1)
    # and inject the resulting list as sub_experiments on the top context.
    pinned_subs = [
        clone_experiment_context(s, partition_number=0, idx=1)
        for s in multi_context.sub_experiments
    ]
    top = multi_context.model_copy(update={"sub_experiments": pinned_subs})

    with capture_dry_run_stdout() as buf:
        RunIdxCommand(experiment_context=top, warming_ratio=2,
                      measurement_ratio=8, use_stdio=False).execute()

    blocks = parse_dry_run_blocks(buf.getvalue())
    leaves = [b for b in blocks if b.cls_name == "RunIdxCommand"]
    assert len(leaves) == 2
    master, follower = leaves
    assert master.started_basename == "RunIdxCommand_part0_idx1_node0.started"
    assert master.done_basename == "RunIdxCommand_part0_idx1_node0.done"
    assert master.waits == []
    assert follower.started_basename == "RunIdxCommand_part0_idx1_node1.started"
    assert follower.done_basename == "RunIdxCommand_part0_idx1_node1.done"
    assert follower.waits_basenames == ["RunIdxCommand_part0_idx1_node0.started"]
