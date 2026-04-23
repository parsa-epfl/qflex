import shutil
import subprocess
import sys
from pathlib import Path

import pytest

from testenv import load_integration_testenv

pytestmark = [
    pytest.mark.integration,
    pytest.mark.requires_testenv,
    pytest.mark.slow,
]


@pytest.fixture(scope="module")
def repo_root() -> Path:
    return Path(__file__).resolve().parents[1]


@pytest.fixture(scope="module")
def qpoints_root(repo_root: Path) -> Path:
    return repo_root / "QPoints"


@pytest.fixture(scope="module")
def integration_env(repo_root: Path):
    return load_integration_testenv(
        repo_root=repo_root,
        required_keys=("qflex_ckp_dir", "gem5_ckp_dir", "core_count", "memory_gb", "base"),
    )


@pytest.fixture(scope="module")
def artifact_paths(integration_env):
    tracked: list[Path] = []
    yield tracked
    if integration_env.keep_artifacts():
        return
    for path in reversed(tracked):
        if path.is_dir():
            shutil.rmtree(path, ignore_errors=True)
        elif path.exists():
            path.unlink()


@pytest.fixture(scope="module")
def writable_base_image(integration_env, artifact_paths, tmp_path_factory) -> Path:
    source = Path(integration_env.get("base"))
    if not source.is_file():
        pytest.skip(f"Base image not found for integration test: {source}")

    image_dir = tmp_path_factory.mktemp("qflex_image")
    image_copy = image_dir / source.name
    shutil.copy2(source, image_copy)
    artifact_paths.append(image_dir)
    return image_copy


@pytest.fixture(scope="module")
def converted_snapshot(repo_root: Path, integration_env, artifact_paths, writable_base_image: Path):
    snapshot = integration_env.snapshot()
    snapshot_dir = Path(integration_env.get("gem5_ckp_dir")) / snapshot
    disk_image = snapshot_dir / f"{snapshot}.img"
    if snapshot_dir.is_dir() and disk_image.is_file():
        artifact_paths.append(snapshot_dir)
        return snapshot_dir

    qflex_script = repo_root / "qflex"
    cmd = [
        sys.executable,
        str(qflex_script),
        "qpoints",
        "convert-single",
        "--qflex-ckp-dir",
        str(integration_env.get("qflex_ckp_dir")),
        "--gem5-ckp-dir",
        str(integration_env.get("gem5_ckp_dir")),
        "--core-count",
        str(integration_env.get("core_count")),
        "--memory-gb",
        str(integration_env.get("memory_gb")),
        "--base",
        str(writable_base_image),
        "--snapshot",
        snapshot,
    ]

    optional_args = {
        "--ssh-host": integration_env.get("ssh_host"),
        "--ssh-user": integration_env.get("ssh_user"),
        "--monitor-base": integration_env.get("monitor_base"),
        "--qmp-base": integration_env.get("qmp_base"),
        "--ssh-base": integration_env.get("ssh_base"),
    }
    for option, value in optional_args.items():
        if value is not None:
            cmd.extend([option, str(value)])

    subprocess.run(cmd, check=True, cwd=repo_root)

    assert snapshot_dir.is_dir()
    assert disk_image.is_file()
    artifact_paths.append(snapshot_dir)
    return snapshot_dir


def _run_qflex_gem5(
    repo_root: Path,
    qpoints_root: Path,
    integration_env,
    artifact_paths,
    experiment: str,
    *extra_args: str,
) -> Path:
    snapshot = integration_env.snapshot()
    outdir = qpoints_root / "sim_outs" / experiment / snapshot
    if outdir.exists():
        shutil.rmtree(outdir)

    cmd = [
        sys.executable,
        str(repo_root / "qflex"),
        "qpoints",
        "run-gem5",
        "--gem5-ckp-dir",
        str(integration_env.get("gem5_ckp_dir")),
        "--experiment",
        experiment,
        "--snapshot",
        snapshot,
        "--inst",
        str(integration_env.insts()),
        "--core-count",
        str(integration_env.get("core_count")),
        *extra_args,
    ]
    subprocess.run(cmd, check=True, cwd=repo_root)
    artifact_paths.append(outdir.parent)
    return outdir


def test_qflex_convert_single_produces_checkpoint_artifacts(converted_snapshot: Path):
    snapshot = converted_snapshot.name
    assert converted_snapshot.is_dir()
    assert (converted_snapshot / f"{snapshot}.img").is_file()


def test_qflex_run_gem5_classic_branch_and_data_trace(
    repo_root: Path,
    qpoints_root: Path,
    integration_env,
    artifact_paths,
    converted_snapshot: Path,
):
    outdir = _run_qflex_gem5(
        repo_root,
        qpoints_root,
        integration_env,
        artifact_paths,
        "pytest_qflex_classic_branch_data_100k",
        "--branch-trace",
        "--data-trace",
    )

    assert (outdir / "stats.txt").is_file()
    assert (outdir / "branch_trace_core_0.log").is_file()
    assert (outdir / "data_trace_core_0.log").is_file()


def test_qflex_run_gem5_classic_sim_config(
    repo_root: Path,
    qpoints_root: Path,
    integration_env,
    artifact_paths,
    converted_snapshot: Path,
    tmp_path: Path,
):
    sim_config = tmp_path / "classic_override.args"
    sim_config.write_text(
        "# exercise additive classic sim-config path\n\n  --mem-ranks=4  \n",
        encoding="utf-8",
    )

    outdir = _run_qflex_gem5(
        repo_root,
        qpoints_root,
        integration_env,
        artifact_paths,
        "pytest_qflex_classic_simconfig_100k",
        "--data-trace",
        "--sim-config",
        str(sim_config),
    )

    assert (outdir / "stats.txt").is_file()
    assert (outdir / "data_trace_core_0.log").is_file()


def test_qflex_run_gem5_ruby_data_trace_cache_dump_and_sim_config(
    repo_root: Path,
    qpoints_root: Path,
    integration_env,
    artifact_paths,
    converted_snapshot: Path,
    tmp_path: Path,
):
    sim_config = tmp_path / "override.args"
    sim_config.write_text("  --mem-ranks=4  \n", encoding="utf-8")

    outdir = _run_qflex_gem5(
        repo_root,
        qpoints_root,
        integration_env,
        artifact_paths,
        "pytest_qflex_ruby_data_dump_100k",
        "--timing-ruby",
        "--data-trace",
        "--dump-cache-state",
        "--sim-config",
        str(sim_config),
    )

    assert (outdir / "stats.txt").is_file()
    assert (outdir / "data_trace_core_0.log").is_file()
    assert (outdir / "ruby_l2cache0_dump.txt").is_file()
