import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

DEFAULT_SNAPSHOT = "snapshot_1"
DEFAULT_INSTS = 100000


def _parse_bool(value, default: bool = False) -> bool:
    if value is None:
        return default
    if isinstance(value, bool):
        return value
    if isinstance(value, (int, float)):
        return bool(value)
    text = str(value).strip().lower()
    if text in {"1", "true", "yes", "y", "on"}:
        return True
    if text in {"0", "false", "no", "n", "off"}:
        return False
    raise RuntimeError(f"Expected a boolean-like value, got: {value!r}")


@dataclass(frozen=True)
class TestEnv:
    path: Path
    values: dict

    def require(self, *keys: str) -> None:
        missing = [key for key in keys if key not in self.values]
        if missing:
            raise RuntimeError(
                f"Test environment file {self.path} is missing required keys: "
                f"{', '.join(missing)}"
            )

    def get(self, key: str, default=None):
        return self.values.get(key, default)

    def snapshot(self) -> str:
        return str(self.get("default_snapshot", DEFAULT_SNAPSHOT))

    def insts(self) -> int:
        return int(self.get("default_insts", DEFAULT_INSTS))

    def keep_artifacts(self) -> bool:
        env_override = os.environ.get("QFLEX_TEST_KEEP_ARTIFACTS")
        if env_override is not None:
            return _parse_bool(env_override)
        return _parse_bool(self.get("keep_artifacts"), default=False)


def candidate_paths(repo_root: Path) -> list[Path]:
    env_override = os.environ.get("QFLEX_TEST_CONFIG")
    candidates = []
    if env_override:
        candidates.append(Path(env_override).expanduser())
    candidates.append(repo_root / ".testenv.json")
    candidates.append(Path.home() / ".config" / "qflex" / "testenv.json")
    return candidates


def resolve_testenv_path(
    repo_root: Path | None = None,
    extra_candidates: Iterable[Path] = (),
) -> Path | None:
    root = repo_root or Path(__file__).resolve().parents[1]
    env_override = os.environ.get("QFLEX_TEST_CONFIG")
    if env_override:
        override_path = Path(env_override).expanduser()
        if not override_path.is_file():
            raise RuntimeError(
                f"QFLEX_TEST_CONFIG points to a missing file: {override_path}"
            )
    for candidate in [*candidate_paths(root), *extra_candidates]:
        if candidate.is_file():
            return candidate
    return None


def load_testenv(
    repo_root: Path | None = None,
    required_keys: Iterable[str] = (),
    extra_candidates: Iterable[Path] = (),
) -> TestEnv:
    path = resolve_testenv_path(repo_root=repo_root, extra_candidates=extra_candidates)
    if path is None:
        raise RuntimeError(
            "No test environment config found. Set QFLEX_TEST_CONFIG or create "
            ".testenv.json in the repo root or ~/.config/qflex/testenv.json."
        )

    values = json.loads(path.read_text(encoding="utf-8"))
    env = TestEnv(path=path, values=values)
    if required_keys:
        env.require(*required_keys)
    return env


def load_integration_testenv(
    repo_root: Path | None = None,
    required_keys: Iterable[str] = (),
    extra_candidates: Iterable[Path] = (),
) -> TestEnv:
    try:
        return load_testenv(
            repo_root=repo_root,
            required_keys=required_keys,
            extra_candidates=extra_candidates,
        )
    except RuntimeError as exc:
        import pytest

        pytest.skip(str(exc))
