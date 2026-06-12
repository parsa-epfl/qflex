"""Tests for the interaction-script Jinja templating (server_cores / client_cores).

Renders through the real render_interaction_script on a real context built by the
factory, against a tmp experiment folder — same code path prepare_for_execution takes.
"""
import os

import pytest

from commands.jinja_loaders.interaction_script_loader import render_interaction_script


@pytest.fixture
def ctx(tmp_path):
    from commands.config import create_experiment_context
    c = create_experiment_context(
        core_count=1, quantum_size=1000, doubled_vcpu=False, llc_size_per_tile_mb=2,
        is_parallel=False, network="none", memory_gb=1, host_name="ZEN3",
        workload_name="web-search", primary_core_start=0, is_consolidated=False,
        primary_ipc=2.0, population_seconds=1, mounting_folder=str(tmp_path),
        experiment_name="tmpl-test",
    )
    os.makedirs(f"{c.get_experiment_folder_address()}/scripts", exist_ok=True)
    return c


def _write_template(tmp_path, body):
    p = tmp_path / "drive.exp.j2"
    p.write_text(body)
    return str(p)


def test_renders_substituted_executable(ctx, tmp_path):
    ctx.server_cores, ctx.client_cores = "0-0", "1-1"
    ctx.interaction_script = _write_template(
        tmp_path, 'send "  --cpuset-cpus={{ server_cores }} \\\\\\r"\n'
                  'send "  --cpuset-cpus={{ client_cores }} \\\\\\r"\n')
    out = render_interaction_script(ctx)
    assert out == f"{ctx.get_experiment_folder_address()}/scripts/drive.exp"
    content = open(out).read()
    assert "--cpuset-cpus=0-0" in content and "--cpuset-cpus=1-1" in content
    assert "{{" not in content
    assert os.access(out, os.X_OK)


def test_rerender_replaces_stale_output(ctx, tmp_path):
    """Every render regenerates the output: a pre-existing rendered file (even with old
    values, even read-only-ish leftovers) is replaced wholesale — no stale reuse."""
    ctx.server_cores, ctx.client_cores = "0-0", "1-1"
    ctx.interaction_script = _write_template(tmp_path, "cores={{ server_cores }}\n")
    out = render_interaction_script(ctx)
    assert "cores=0-0" in open(out).read()
    # New run, new value: same output path must carry the fresh render.
    ctx.server_cores = "0-3"
    out2 = render_interaction_script(ctx)
    assert out2 == out
    content = open(out2).read()
    assert "cores=0-3" in content and "0-0" not in content
    assert not [p for p in os.listdir(os.path.dirname(out)) if ".tmp." in p]


def test_missing_variable_names_it(ctx, tmp_path):
    ctx.server_cores = "0-0"  # client_cores left unset
    ctx.interaction_script = _write_template(tmp_path, "x {{ client_cores }} y")
    with pytest.raises(ValueError, match=r"client_cores.*--client-cores"):
        render_interaction_script(ctx)


def test_unsupported_variable_rejected(ctx, tmp_path):
    ctx.interaction_script = _write_template(tmp_path, "x {{ not_a_var }} y")
    with pytest.raises(ValueError, match="unsupported.*not_a_var"):
        render_interaction_script(ctx)


def test_non_j2_path_untouched(ctx, tmp_path):
    p = tmp_path / "plain.exp"
    p.write_text("expect stuff")
    ctx.interaction_script = str(p)
    assert render_interaction_script(ctx) == str(p)


def test_real_ws_templates_parse_and_render(ctx):
    """The three shipped WS .exp.j2 templates must survive Jinja (Tcl braces are single,
    so Jinja's {{ }} never collides) and render with both vars provided."""
    ctx.server_cores, ctx.client_cores = "0-0", "1-1"
    for rel in [
        "conf/WS/single_node/expects/load_savevm_loaded_single.exp.j2",
        "conf/WS/multi_node/expects/load_savevm_loaded_master.exp.j2",
        "conf/WS/multi_node/expects/load_savevm_loaded_wait.exp.j2",
    ]:
        ctx.interaction_script = rel
        out = render_interaction_script(ctx)
        content = open(out).read()
        assert "{{" not in content and "{%" not in content
        assert "--cpuset-cpus=0-0" in content or "--cpuset-cpus=1-1" in content
