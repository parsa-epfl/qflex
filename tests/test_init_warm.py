"""Tests for the InitWarm phase."""
import os

from .conftest import (
    capture_dry_run_stdout,
    parse_dry_run_blocks,
    assert_two_node_master_first,
)


def test_init_warm_two_nodes(multi_context, mock_mounting_folder):
    """InitWarm renders Jinja templates inside cmd(); we skip that work via
    skip_generate_cfg=True so the test just verifies the dispatch shape.
    A pre-existing parameter.rs is required for that path."""
    for sub_name in ("data-caching-comparison-node-0", "data-caching-comparison-node-1"):
        cfg = f"{mock_mounting_folder}/experiments/{sub_name}/cfg"
        os.makedirs(cfg, exist_ok=True)
        open(f"{cfg}/parameter.rs", "w").close()

    from commands.init_warm import InitWarm
    with capture_dry_run_stdout() as buf:
        InitWarm(experiment_context=multi_context, skip_generate_cfg=True).execute()
    blocks = parse_dry_run_blocks(buf.getvalue())
    assert_two_node_master_first(blocks, "InitWarm", "InitWarm_node")
