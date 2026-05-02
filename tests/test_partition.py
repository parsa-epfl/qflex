"""Tests for the partition-management phases: PartitionCommand, CleanPartitionCommand,
UnPartitionCommand. These are leaf executors with no internal parallelism — the
only parallelism is the multi-node sub_experiments dispatch."""
import os
import shutil

from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
)


def test_partition_two_nodes(multi_context, mock_mounting_folder):
    """PartitionCommand wraps each node's partition-creation script. We have to
    remove the pre-existing partition_* dirs first so PartitionCommand doesn't bail
    with 'partitions already exist' (the fixture creates them for the run-* tests;
    they don't belong here)."""
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
