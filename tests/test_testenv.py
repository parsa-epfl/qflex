import json

import pytest

from testenv import (
    DEFAULT_INSTS,
    DEFAULT_SNAPSHOT,
    load_testenv,
    resolve_testenv_path,
)


def test_resolve_testenv_prefers_env_override(tmp_path, monkeypatch):
    override = tmp_path / "override.json"
    override.write_text("{}", encoding="utf-8")
    monkeypatch.setenv("QFLEX_TEST_CONFIG", str(override))

    found = resolve_testenv_path(repo_root=tmp_path)

    assert found == override


def test_resolve_testenv_rejects_missing_env_override(tmp_path, monkeypatch):
    missing = tmp_path / "missing.json"
    monkeypatch.setenv("QFLEX_TEST_CONFIG", str(missing))

    with pytest.raises(RuntimeError, match="QFLEX_TEST_CONFIG points to a missing file"):
        resolve_testenv_path(repo_root=tmp_path)


def test_load_testenv_from_repo_local_file(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    config = tmp_path / ".testenv.json"
    config.write_text(json.dumps({"snapshot": "snapshot_0"}), encoding="utf-8")

    env = load_testenv(repo_root=tmp_path, required_keys=("snapshot",))

    assert env.path == config
    assert env.get("snapshot") == "snapshot_0"


def test_load_testenv_reports_missing_required_keys(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    config = tmp_path / ".testenv.json"
    config.write_text("{}", encoding="utf-8")

    with pytest.raises(RuntimeError, match="missing required keys"):
        load_testenv(repo_root=tmp_path, required_keys=("gem5_ckp_dir",))


def test_load_testenv_uses_integration_defaults(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    config = tmp_path / ".testenv.json"
    config.write_text("{}", encoding="utf-8")

    env = load_testenv(repo_root=tmp_path)

    assert env.snapshot() == DEFAULT_SNAPSHOT
    assert env.insts() == DEFAULT_INSTS


def test_keep_artifacts_defaults_to_false(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    monkeypatch.delenv("QFLEX_TEST_KEEP_ARTIFACTS", raising=False)
    config = tmp_path / ".testenv.json"
    config.write_text("{}", encoding="utf-8")

    env = load_testenv(repo_root=tmp_path)

    assert env.keep_artifacts() is False


def test_keep_artifacts_can_be_enabled_by_config(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    monkeypatch.delenv("QFLEX_TEST_KEEP_ARTIFACTS", raising=False)
    config = tmp_path / ".testenv.json"
    config.write_text(json.dumps({"keep_artifacts": True}), encoding="utf-8")

    env = load_testenv(repo_root=tmp_path)

    assert env.keep_artifacts() is True


def test_keep_artifacts_env_override_wins(tmp_path, monkeypatch):
    monkeypatch.delenv("QFLEX_TEST_CONFIG", raising=False)
    monkeypatch.setenv("QFLEX_TEST_KEEP_ARTIFACTS", "1")
    config = tmp_path / ".testenv.json"
    config.write_text(json.dumps({"keep_artifacts": False}), encoding="utf-8")

    env = load_testenv(repo_root=tmp_path)

    assert env.keep_artifacts() is True
