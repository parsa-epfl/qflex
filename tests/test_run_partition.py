"""Tests for RunPartitionCommand — the dynamic-sub_experiments orchestrator that
fans the partition axis out via mp.Process (replacing the old ParallelExecutor).
Includes:
- The full multi-node × per-partition × per-idx tree dispatch (dry-run).
- A wall-clock test verifying partitions actually run in parallel (real subprocess
  with monkeypatched leaves so total elapsed reflects per-partition concurrency).
"""
import time

from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
)


def test_run_partition_runs_partitions_in_parallel(multi_context_for_phase, monkeypatch):
    """Wall-clock proof that RunPartitionCommand fans the partition axis out in
    parallel via mp.Process — not sequentially.

    The fixture has 2 partitions × 2 idxs per node. We pin to a single node
    (the master sub-experiment) so this test exercises only the per-partition
    parallelism, independent of the multi-node axis. Each idx leaf is patched
    to a 1-second sleep, and the inter-idx SimpleCMDExecutor("sleep 5") is
    short-circuited. So:

      * 2 partitions × 2 idxs sequential within each partition = 2s if partitions are PARALLEL.
      * 2 partitions × 2 idxs all serialized                   = 4s if partitions are SEQUENTIAL.

    A 3.5s ceiling distinguishes the two cleanly.
    """
    from commands.run_partition import RunPartitionCommand
    from commands.config import ExperimentContext
    from commands.run_idx import RunIdxCommand
    from commands.executer import Executor, SimpleCMDExecutor

    # Skip leaf prep (would try to copy non-existent QEMU binaries) and shm cleanup.
    monkeypatch.setattr(ExperimentContext, "prepare_for_execution", lambda self: None)
    monkeypatch.setattr(ExperimentContext, "clean_up", lambda self: None)
    # Each idx is now a 1-second sleep instead of a real QEMU run.
    monkeypatch.setattr(RunIdxCommand, "cmd", lambda self: ["sleep 1"])
    # Skip the inter-idx 5s "let the system recover" sleeps and the "rm output_state".
    monkeypatch.setattr(SimpleCMDExecutor, "cmd", lambda self: ":")
    # Multi-node executor has a 30s post-exit grace before peer cleanup; with
    # this sub-experiment having neighbor_node_list=[1] each leaf would burn
    # the full 30s, blowing the wall-clock budget. Zero it for this test —
    # we're testing dispatch parallelism, not the grace.
    monkeypatch.setattr(Executor, "POST_EXIT_GRACE_SECONDS", 0)

    multi_context = multi_context_for_phase("run_partition")
    master = multi_context.sub_experiments[0]
    runner = RunPartitionCommand(experiment_context=master)

    start = time.monotonic()
    ok = runner.execute(to_stdio=False)
    elapsed = time.monotonic() - start

    assert ok is True
    # Sanity: idxs ARE sequential within a partition, so wall time must be at least ~2s.
    assert elapsed > 1.5, (
        f"Idxs within a partition should run sequentially; expected >2s, got {elapsed:.2f}s"
    )
    # The whole point: partitions run in parallel, not sequentially.
    # Sequential would be ~4s. Parallel should be ~2s plus mp.Process overhead.
    assert elapsed < 3.5, (
        f"Partitions should run in parallel; expected ~2s, got {elapsed:.2f}s. "
        f"At 4s+ they're being serialized."
    )


def test_run_partition_full_tree_two_nodes(multi_context_for_phase):
    """RunPartitionCommand on the multi-node group: nodes parallel, partitions
    parallel (within each node), idxs sequential (within each partition).
    All three axes use the same sub_experiments dispatch (the former ParallelExecutor
    is gone). Verify that for every (node, partition, idx) tuple, the master
    appears before the follower in dispatch order. warming_ratio /
    measurement_ratio come from the run_partition phase overlay in
    dc-multi.yaml."""
    from commands.run_partition import RunPartitionCommand

    multi_context = multi_context_for_phase("run_partition")
    with capture_dry_run_stdout() as buf:
        RunPartitionCommand(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())

    leaves = [b for b in blocks if b.cls_name == "RunIdxCommand"]
    # 2 nodes × 2 partitions × 2 idxs = 8 leaves.
    assert len(leaves) == 8, f"expected 8 leaves, got {len(leaves)}: {[l.started_basename for l in leaves]}"

    # Pair them by (part, idx) and assert master-first.
    assert_two_node_master_first(blocks, "RunIdxCommand", "RunIdxCommand_part")

    # Within a node-partition, idxs run sequentially (idx 0 dispatches before idx 1).
    for node in (0, 1):
        for part in (0, 1):
            i0 = next(b for b in leaves
                      if b.started_basename == f"RunIdxCommand_part{part}_idx0_node{node}.started")
            i1 = next(b for b in leaves
                      if b.started_basename == f"RunIdxCommand_part{part}_idx1_node{node}.started")
            assert blocks.index(i0) < blocks.index(i1), (
                f"node {node} partition {part}: idx 0 must dispatch before idx 1"
            )
