"""FullTimingCommand — one continuous timing run per node from init_warmed: a `partition 1` child
(links + run_partitions.sh) followed by one FullTimingIdx leaf at (part 0, idx 0) that loads
init_warmed with the full-timing cycle budget. Master-first coordination comes from the leaf
sentinels exactly as for run-idx."""
import os
import shutil

from .conftest import capture_dry_run_stdout, parse_dry_run_blocks


def test_full_timing_two_nodes(multi_context_for_phase, mock_mounting_folder):
    from commands.full_timing import FullTimingCommand
    from commands.config import clone_experiment_context

    # The fixture pre-creates partitions for the run-* tests; full-timing's `partition 1` refuses if they exist.
    for sub_name in ("data-caching-comparison-node-0", "data-caching-comparison-node-1"):
        run = f"{mock_mounting_folder}/experiments/{sub_name}/run"
        for p in os.listdir(run):
            if p.startswith("partition_"):
                shutil.rmtree(f"{run}/{p}")

    multi_context = multi_context_for_phase("run_idx")
    # Through the factory (clone), not model_copy: FullTimingCommand clones its leaves from _creation_kwargs.
    subs = [clone_experiment_context(s, full_timing=True, full_timing_cycles=10_000_000)
            for s in multi_context.sub_experiments]
    top = multi_context.model_copy(update={"sub_experiments": subs})

    with capture_dry_run_stdout() as buf:
        FullTimingCommand(experiment_context=top).execute()

    blocks = parse_dry_run_blocks(buf.getvalue())
    partitions = [b for b in blocks if b.cls_name == "PartitionCommand"]
    leaves = [b for b in blocks if b.cls_name == "FullTimingIdx"]
    assert len(partitions) == 2 and len(leaves) == 2
    assert all("partition.py 1" in b.bash for b in partitions)

    master, follower = leaves
    assert master.started_basename == "FullTimingIdx_part0_idx0_node0.started"
    assert master.waits == []
    assert follower.waits_basenames == ["FullTimingIdx_part0_idx0_node0.started"]
    interval = subs[0].stat_interval_cycles
    for b in leaves:
        assert "-loadvm init_warmed,on-demand" in b.bash
        assert "tmp-snapshot-name=init_warmed" in b.bash
        assert "./checkpoint_conversion ./init_warmed.uarch" in b.bash
        assert "ckpt-path=./init_warmed-flexus" in b.bash
        assert f"cycles=10000001:{interval}" in b.bash
        assert "snapshot_0" not in b.bash


def test_full_timing_cycles_default_is_population(multi_context_for_phase):
    sub = multi_context_for_phase("run_idx").sub_experiments[0]
    expected = int(sub.workload.population * sub.workload.IPC_info.machine_freq_ghz) + 1
    assert sub.full_timing_total_cycles() == expected
    assert sub.model_copy(update={"full_timing_cycles": 123}).full_timing_total_cycles() == 124
