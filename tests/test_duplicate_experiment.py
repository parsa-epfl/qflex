"""Tests for `./qflex duplicate-experiment` — pure filesystem copy, no QEMU.

Drives the real CLI via subprocess (same code path production uses) against
self-contained YAMLs written into a tmp mounting folder.
"""
import os
import subprocess
import sys

import pytest

from .conftest import REPO_ROOT

QFLEX_PATH = os.path.join(REPO_ROOT, "qflex")

LEAF_DEFAULTS = """\
_leaf_defaults:
  _target_: commands.config.create_experiment_context
  quantum_size: 1000
  core_count: 1
  doubled_vcpu: false
  llc_size_per_tile_mb: 2
  is_parallel: true
  network: none
  memory_gb: 64
  host_name: ZEN3
  workload_name: data-caching
  population_seconds: 5
  is_consolidated: false
  primary_ipc: 2.0
  primary_core_start: 0
  image_folder: {mf}
  mounting_folder: {mf}
  image_name: root-single-node.qcow2
  keep_experiment_unique: false
  use_image_directly: true
"""

MULTI_YAML = LEAF_DEFAULTS + """\
components:
  experiment_context:
    _base_: _leaf_defaults
    experiment_name: {name}
    _deps_:
      sub_experiments:
        - name: node_0
        - name: node_1
  experiment_context_node_0:
    _base_: _leaf_defaults
    _name_: node_0
    experiment_name: {name}-node-0
    node_number: 0
    neighbor_node_list: [1]
    pdes_net_devs: ["virtio-net-pci"]
    latencies_ns_list: [100000]
    syncs_list: ["true"]
  experiment_context_node_1:
    _base_: _leaf_defaults
    _name_: node_1
    experiment_name: {name}-node-1
    node_number: 1
    neighbor_node_list: [0]
    pdes_net_devs: ["virtio-net-pci"]
    latencies_ns_list: [100000]
    syncs_list: ["true"]
    wait_for_nodes: [0]
"""

SINGLE_YAML = LEAF_DEFAULTS + """\
components:
  experiment_context:
    _base_: _leaf_defaults
    experiment_name: {name}
"""


def _write_yaml(tmp_path, fname, template, name):
    path = str(tmp_path / fname)
    with open(path, "w") as f:
        f.write(template.format(mf=str(tmp_path), name=name))
    return path


def _make_source_folders(tmp_path, names):
    for n in names:
        folder = tmp_path / "experiments" / n
        os.makedirs(folder / "run")
        (folder / "marker.txt").write_text(n)
        os.symlink(f"{tmp_path}/{n}.qcow2", folder / "run" / "image.qcow2")


def _run(args):
    return subprocess.run(
        [sys.executable, QFLEX_PATH, "duplicate-experiment", *args],
        cwd=REPO_ROOT, capture_output=True, text=True, timeout=60,
    )


SRC_NAMES = ["exp-src", "exp-src-node-0", "exp-src-node-1"]
DST_NAMES = ["exp-dst", "exp-dst-node-0", "exp-dst-node-1"]


@pytest.fixture
def yamls(tmp_path):
    _make_source_folders(tmp_path, SRC_NAMES)
    src = _write_yaml(tmp_path, "src.yaml", MULTI_YAML, "exp-src")
    dst = _write_yaml(tmp_path, "dst.yaml", MULTI_YAML, "exp-dst")
    return tmp_path, src, dst


def test_duplicates_multi_node_folders(yamls):
    tmp_path, src, dst = yamls
    r = _run(["-c", dst, "--target-yaml", src])
    assert r.returncode == 0, r.stderr
    for s, d in zip(SRC_NAMES, DST_NAMES):
        folder = tmp_path / "experiments" / d
        assert (folder / "marker.txt").read_text() == s
        assert os.path.islink(folder / "run" / "image.qcow2")


def test_existing_destination_errors(yamls):
    tmp_path, src, dst = yamls
    os.makedirs(tmp_path / "experiments" / "exp-dst-node-1")
    r = _run(["-c", dst, "--target-yaml", src])
    assert r.returncode != 0
    assert "exp-dst-node-1" in r.stderr
    # nothing copied — the check runs before any copy
    assert not os.path.exists(tmp_path / "experiments" / "exp-dst")


def test_overwrite_keeps_extra_dest_files(yamls):
    tmp_path, src, dst = yamls
    extra = tmp_path / "experiments" / "exp-dst" / "extra.txt"
    os.makedirs(extra.parent)
    extra.write_text("keep me")
    (extra.parent / "marker.txt").write_text("stale")
    r = _run(["-c", dst, "--target-yaml", src, "--overwrite"])
    assert r.returncode == 0, r.stderr
    assert extra.read_text() == "keep me"
    assert (extra.parent / "marker.txt").read_text() == "exp-src"


def test_replace_removes_old_dest(yamls):
    tmp_path, src, dst = yamls
    extra = tmp_path / "experiments" / "exp-dst" / "extra.txt"
    os.makedirs(extra.parent)
    extra.write_text("stale")
    r = _run(["-c", dst, "--target-yaml", src, "--replace"])
    assert r.returncode == 0, r.stderr
    assert not extra.exists()
    assert (extra.parent / "marker.txt").read_text() == "exp-src"


def test_overwrite_and_replace_mutually_exclusive(yamls):
    _, src, dst = yamls
    r = _run(["-c", dst, "--target-yaml", src, "--overwrite", "--replace"])
    assert r.returncode != 0
    assert "mutually exclusive" in r.stderr


def test_hierarchy_mismatch_errors(yamls):
    tmp_path, src, _ = yamls
    dst_single = _write_yaml(tmp_path, "dst-single.yaml", SINGLE_YAML, "exp-dst")
    r = _run(["-c", dst_single, "--target-yaml", src])
    assert r.returncode != 0
    assert "hierarchies differ" in r.stderr.lower()


def test_missing_source_folder_errors(tmp_path):
    src = _write_yaml(tmp_path, "src.yaml", MULTI_YAML, "exp-src")
    dst = _write_yaml(tmp_path, "dst.yaml", MULTI_YAML, "exp-dst")
    r = _run(["-c", dst, "--target-yaml", src])
    assert r.returncode != 0
    assert "not found" in r.stderr
