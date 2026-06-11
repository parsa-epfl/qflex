import json
from pathlib import Path

from .config import ExperimentContext


_INIT_WARMED_POLICY_FILE = "init_warmed.gem5_bbl_btb.json"


def _policy_path(experiment_context: ExperimentContext) -> Path:
    experiment_dir = Path(experiment_context.get_experiment_folder_address())
    return experiment_dir / "run" / _INIT_WARMED_POLICY_FILE


def record_init_warmed_bbl_btb_policy(
    experiment_context: ExperimentContext,
    collect_gem5_bbl_btb: bool,
) -> None:
    path = _policy_path(experiment_context)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps({"collect_gem5_bbl_btb": collect_gem5_bbl_btb}, indent=2) + "\n",
        encoding="utf-8",
    )


def read_init_warmed_bbl_btb_policy(
    experiment_context: ExperimentContext,
) -> bool | None:
    path = _policy_path(experiment_context)
    if not path.exists():
        return None
    try:
        data = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None
    if not isinstance(data, dict):
        return None
    value = data.get("collect_gem5_bbl_btb")
    if isinstance(value, bool):
        return value
    return None


def warn_if_init_warmed_bbl_btb_policy_mismatches(
    experiment_context: ExperimentContext,
    collect_gem5_bbl_btb: bool,
) -> None:
    if experiment_context.loadvm_name != "init_warmed":
        return

    prior = read_init_warmed_bbl_btb_policy(experiment_context)
    if prior is None or prior == collect_gem5_bbl_btb:
        return

    if prior and not collect_gem5_bbl_btb:
        print(
            "Warning: init_warmed was created with gem5 BBL-BTB collection enabled, "
            "but current fw has it disabled. Later snapshots will stop extending the "
            "gem5 BBL-BTB export from this point onward."
        )
        return

    print(
        "Warning: init_warmed was created without gem5 BBL-BTB collection, but current "
        "fw has it enabled. Later snapshots will start gem5 BBL-BTB collection only "
        "from the fw phase onward."
    )
