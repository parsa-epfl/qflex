"""Multi-node ordering tests.

For each pipeline phase, build the corresponding executor on a 2-node multi config,
run it in dry-run, and assert:

* the master node's leaf is dispatched before the non-master's,
* the master leaf has no [py-wait]; the non-master leaf has exactly one [py-wait]
  pointing at the master's matching .started sentinel,
* sentinel basenames carry the correct (phase, partition, idx, node) namespace,
* the master's .done [py-touch] is recorded after the bash line.

The dry-run code path iterates sub_experiments sequentially in the parent process
(no mp.Process spawn), so the captured stdout reflects the exact dispatch order.
"""
import os

from .conftest import capture_dry_run_stdout, parse_dry_run_blocks


# --- helpers ----------------------------------------------------------------

def assert_two_node_master_first(blocks, leaf_cls_name: str,
                                 expected_basename_prefix: str):
    """Assert the standard 2-node leaf-ordering invariant on a list of dry-run blocks
    that *end* with two master+node-1 leaves of the given class."""
    leaves = [b for b in blocks if b.cls_name == leaf_cls_name]
    assert len(leaves) >= 2, (
        f"expected at least 2 {leaf_cls_name} leaves, got {len(leaves)}: "
        f"{[b.cls_name for b in blocks]}"
    )

    # Pair up master + node-1 leaves by sentinel suffix (everything except _nodeN).
    by_pair: dict[str, dict[int, object]] = {}
    for b in leaves:
        assert b.started is not None, f"missing .started touch on {b.cls_name}: {b.raw}"
        base = b.started_basename
        # base looks like e.g. "RunIdxCommand_part0_idx1_node0.started"
        assert base.startswith(expected_basename_prefix), (
            f"{b.cls_name} sentinel '{base}' missing expected prefix "
            f"'{expected_basename_prefix}'"
        )
        # extract the leading shared key (everything before _nodeN.started)
        key = base.rsplit("_node", 1)[0]
        node = int(base.rsplit("_node", 1)[1].split(".")[0])
        by_pair.setdefault(key, {})[node] = b

    assert by_pair, "found no master/node-1 leaf pairs"
    for key, nodes in by_pair.items():
        assert 0 in nodes and 1 in nodes, (
            f"missing pair member for key {key!r}: have nodes {list(nodes)}"
        )
        master, follower = nodes[0], nodes[1]

        # Master leaf: no waits, .started touch, bash, .done touch.
        assert master.waits == [], (
            f"master leaf for {key} should not wait, has {master.waits}"
        )
        assert master.started_basename.endswith("_node0.started")
        assert master.bash, f"master leaf for {key} has empty bash"
        assert master.done is not None, f"master leaf for {key} missing .done"
        assert master.done_basename.endswith("_node0.done")

        # Non-master leaf: waits on master's .started, then its own touch.
        assert len(follower.waits) == 1, (
            f"follower leaf for {key} should wait on exactly one sentinel, "
            f"got {follower.waits}"
        )
        assert follower.waits_basenames[0] == master.started_basename, (
            f"follower for {key} waits on {follower.waits_basenames[0]}, "
            f"expected {master.started_basename}"
        )
        assert follower.started_basename.endswith("_node1.started")
        assert follower.done is not None
        assert follower.done_basename.endswith("_node1.done")

    # Master appears before follower in the dispatch order (dry-run is sequential).
    for key, nodes in by_pair.items():
        master_idx = blocks.index(nodes[0])
        follower_idx = blocks.index(nodes[1])
        assert master_idx < follower_idx, (
            f"for {key}, master leaf at idx {master_idx} should precede "
            f"follower leaf at idx {follower_idx}"
        )


# --- phase tests ------------------------------------------------------------

def test_boot_two_nodes(multi_context):
    from commands.boot import Boot
    with capture_dry_run_stdout() as buf:
        Boot(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Boot", "Boot_node")


def test_load_two_nodes(multi_context):
    from commands.load import Load
    with capture_dry_run_stdout() as buf:
        Load(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "Load", "Load_node")


def test_init_warm_two_nodes(multi_context, mock_mounting_folder):
    """init_warm renders Jinja templates inside cmd(); we skip that work via
    skip_generate_cfg=True so the test just verifies the dispatch shape.
    A pre-existing parameter.rs is required for that path."""
    for sub_name in ("data-caching_yaml_node_0", "data-caching_yaml_node_1"):
        cfg = f"{mock_mounting_folder}/experiments/{sub_name}/cfg"
        os.makedirs(cfg, exist_ok=True)
        open(f"{cfg}/parameter.rs", "w").close()

    from commands.init_warm import InitWarm
    with capture_dry_run_stdout() as buf:
        InitWarm(experiment_context=multi_context, skip_generate_cfg=True).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "InitWarm", "InitWarm_node")


def test_functional_warming_two_nodes(multi_context):
    from commands.functional_warming import FunctionalWarming
    with capture_dry_run_stdout() as buf:
        FunctionalWarming(experiment_context=multi_context, sample_size=10).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "FunctionalWarming", "FunctionalWarming_node")


def test_partition_two_nodes(multi_context, mock_mounting_folder):
    """PartitionCommand wraps each node's partition-creation script; we have to
    remove the pre-existing partition_* dirs so PartitionCommand doesn't bail with
    'partitions already exist'. The fixture creates them for the run-* tests; they
    don't belong here."""
    import shutil
    for sub_name in ("data-caching_yaml_node_0", "data-caching_yaml_node_1"):
        run = f"{mock_mounting_folder}/experiments/{sub_name}/run"
        for p in os.listdir(run):
            if p.startswith("partition_"):
                shutil.rmtree(f"{run}/{p}")

    from commands.partition import PartitionCommand
    with capture_dry_run_stdout() as buf:
        PartitionCommand(experiment_context=multi_context, partition_count=4).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "PartitionCommand", "PartitionCommand_node")


def test_clean_partition_two_nodes(multi_context):
    from commands.partition import CleanPartitionCommand
    with capture_dry_run_stdout() as buf:
        CleanPartitionCommand(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "CleanPartitionCommand", "CleanPartitionCommand_node")


def test_unpartition_two_nodes(multi_context):
    from commands.partition import UnPartitionCommand
    with capture_dry_run_stdout() as buf:
        UnPartitionCommand(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "UnPartitionCommand", "UnPartitionCommand_node")


def test_result_two_nodes(multi_context):
    from commands.result import RunResultCommand
    with capture_dry_run_stdout() as buf:
        RunResultCommand(experiment_context=multi_context).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "RunResultCommand", "RunResultCommand_node")


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


def test_run_single_partition_two_nodes(multi_context):
    """RunSinglePartitionCommand at a fixed partition_number. Each node iterates
    its idxs sequentially via SequentialGroupExecutor; at the per-(partition, idx)
    leaf the master goes first, then the follower."""
    from commands.run_single_partition import RunSinglePartitionCommand
    from commands.config import clone_experiment_context

    pinned_subs = [
        clone_experiment_context(s, partition_number=0)
        for s in multi_context.sub_experiments
    ]
    top = multi_context.model_copy(update={"sub_experiments": pinned_subs})

    with capture_dry_run_stdout() as buf:
        RunSinglePartitionCommand(experiment_context=top, warming_ratio=2,
                                  measurement_ratio=8, use_stdio=False).execute()
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


def test_run_partition_runs_partitions_in_parallel(multi_context, monkeypatch):
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
    import time
    from commands.run_partition import RunPartitionCommand
    from commands.config import ExperimentContext
    from commands.run_idx import RunIdxCommand
    from commands.executer import SimpleCMDExecutor

    # Skip leaf prep (would try to copy non-existent QEMU binaries) and shm cleanup.
    monkeypatch.setattr(ExperimentContext, "prepare_for_execution", lambda self: None)
    monkeypatch.setattr(ExperimentContext, "clean_up", lambda self: None)
    # Each idx is now a 1-second sleep instead of a real QEMU run.
    monkeypatch.setattr(RunIdxCommand, "cmd", lambda self: ["sleep 1"])
    # Skip the inter-idx 5s "let the system recover" sleeps and the "rm output_state".
    monkeypatch.setattr(SimpleCMDExecutor, "cmd", lambda self: ":")

    master = multi_context.sub_experiments[0]
    runner = RunPartitionCommand(experiment_context=master,
                                 warming_ratio=2, measurement_ratio=8)

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


def test_run_partition_full_tree_two_nodes(multi_context):
    """RunPartitionCommand on the multi-node group: nodes parallel, partitions
    parallel (within each node), idxs sequential (within each partition).
    All three axes use the same sub_experiments dispatch (the former ParallelExecutor
    is gone). Verify that for every (node, partition, idx) tuple, the master
    appears before the follower in dispatch order."""
    from commands.run_partition import RunPartitionCommand

    with capture_dry_run_stdout() as buf:
        RunPartitionCommand(experiment_context=multi_context, warming_ratio=2,
                            measurement_ratio=8).execute()
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
