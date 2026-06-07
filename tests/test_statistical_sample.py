"""Tests for the statistical-sample orchestrator (`./qflex statistical-sample`).

The four phase commands are monkeypatched so no real QEMU/Flexus runs: the fakes
just reproduce the side effects the orchestrator depends on (fw materialises
run/core_info.csv; result writes core_info_new.csv + REQUIRED_SAMPLE_SIZE)."""
import os

import pytest

from commands.statistical_sample import StatisticalSampleCommand
from commands.functional_warming import FunctionalWarming
from commands.partition import PartitionCommand, CleanPartitionCommand
from commands.run_partition import RunPartitionCommand
from commands.result import RunResultCommand


def _leaves(ctx):
    return list(ctx.sub_experiments) if ctx.has_sub_experiments() else [ctx]


def _patch_phases(monkeypatch, required_by_iteration):
    """Fake the four phases. `required_by_iteration` is a list (one per iteration)
    of {node_number: required_sample_size} dicts that the fake result step writes."""
    state = {"iter": 0, "sample_sizes_seen": []}

    def fake_fw(self, *a, **k):
        # Real get_ipns_csv: creates the base core_info_<sample_size>.csv and points
        # core_info.csv at the biggest size (what the executor does at phase startup).
        for leaf in _leaves(self.experiment_context):
            leaf.get_ipns_csv()
        state["sample_sizes_seen"].append(self.experiment_context.sample_size)
        return True

    def fake_noop(self, *a, **k):
        return True

    def fake_result(self, *a, **k):
        # Fake only the subprocess result scripts (core_info_new.csv + REQUIRED), then
        # call the REAL RunResultCommand._save_next_core_info so its cross-node save logic
        # is exercised.
        reqs = required_by_iteration[state["iter"]]
        for leaf in _leaves(self.experiment_context):
            folder = leaf.get_experiment_folder_address()
            with open(f"{folder}/core_info_new.csv", "w") as f:
                f.write("ipns\n2.0\n")
            with open(f"{folder}/REQUIRED_SAMPLE_SIZE", "w") as f:
                f.write(str(reqs[leaf.node_number]))
            open(f"{folder}/RunResultCommand.log", "w").close()
        # Mirror the real execute(): only save when the flag is on (the loop forces it).
        if self.experiment_context.save_next_core_info:
            self._save_next_core_info()
        state["iter"] += 1
        return True

    monkeypatch.setattr(FunctionalWarming, "execute", fake_fw)
    monkeypatch.setattr(PartitionCommand, "execute", fake_noop)
    monkeypatch.setattr(RunPartitionCommand, "execute", fake_noop)
    monkeypatch.setattr(CleanPartitionCommand, "execute", fake_noop)
    monkeypatch.setattr(RunResultCommand, "execute", fake_result)
    return state


def test_fresh_start_round_to_50_and_converge(monkeypatch, multi_context):
    """Fresh start seeds core_info_<S0>; loop rounds the next size up to 50 and
    converges once every node's required <= current."""
    s0 = multi_context.sample_size
    leaves = _leaves(multi_context)
    # iter1: node0 needs 120 (> s0) -> next=150; iter2: both <=150 -> converge.
    state = _patch_phases(monkeypatch, [{0: 120, 1: 80}, {0: 140, 1: 130}])

    ok = StatisticalSampleCommand(experiment_context=multi_context, max_iterations=10).execute()

    assert ok is True
    assert state["sample_sizes_seen"] == [s0, 150]  # fw ran at s0 then at the rounded-to-50 size
    for leaf in leaves:
        run = f"{leaf.get_experiment_folder_address()}/run"
        assert os.path.exists(f"{run}/core_info_{s0}.csv")   # seeded from yaml/cli values
        assert os.path.exists(f"{run}/core_info_150.csv")    # next size's refined input
        # per-iteration log preserved, not overwritten
        assert os.path.exists(f"{leaf.get_experiment_folder_address()}/RunResultCommand_{s0}.log")


def test_next_size_is_max_across_nodes(monkeypatch, multi_context):
    """The next sample size derives from the MAX required across nodes."""
    state = _patch_phases(monkeypatch, [{0: 90, 1: 410}, {0: 100, 1: 420}])

    ok = StatisticalSampleCommand(experiment_context=multi_context, max_iterations=10).execute()

    assert ok is True
    # node1's 410 dominates -> ceil(410/50)*50 = 450; converges next iter (both <=450).
    assert state["sample_sizes_seen"][-1] == 450


def test_resume_from_biggest_existing(monkeypatch, multi_context):
    """Resume starts at the biggest core_info_<size>.csv already on disk."""
    leaves = _leaves(multi_context)
    for leaf in leaves:
        run = f"{leaf.get_experiment_folder_address()}/run"
        os.makedirs(run, exist_ok=True)
        for s in (10, 500):
            with open(f"{run}/core_info_{s}.csv", "w") as f:
                f.write("ipns\n1.0\n")
    # Already enough at 500 -> converge immediately, no new size.
    state = _patch_phases(monkeypatch, [{0: 400, 1: 480}])

    ok = StatisticalSampleCommand(experiment_context=multi_context, max_iterations=10).execute()

    assert ok is True
    assert state["sample_sizes_seen"] == [500]  # started at the biggest, not s0


def test_mismatched_sizes_across_nodes_error(monkeypatch, multi_context):
    """Multi-node: differing latest core_info_<size>.csv across nodes is an error."""
    leaves = _leaves(multi_context)
    sizes = (300, 500)
    for leaf, s in zip(leaves, sizes):
        run = f"{leaf.get_experiment_folder_address()}/run"
        os.makedirs(run, exist_ok=True)
        with open(f"{run}/core_info_{s}.csv", "w") as f:
            f.write("ipns\n1.0\n")
    _patch_phases(monkeypatch, [{0: 1, 1: 1}])

    with pytest.raises(RuntimeError, match="differs across nodes"):
        StatisticalSampleCommand(experiment_context=multi_context, max_iterations=10).execute()


def test_max_iterations_guard(monkeypatch, multi_context):
    """Never-converging required size stops at the iteration cap."""
    # required grows past current every iteration -> never satisfied.
    state = _patch_phases(monkeypatch, [{0: 10**6, 1: 10**6}, {0: 10**7, 1: 10**7}, {0: 10**8, 1: 10**8}])

    ok = StatisticalSampleCommand(experiment_context=multi_context, max_iterations=3).execute()

    assert ok is False
    assert state["iter"] == 3


def test_get_ipns_csv_points_core_info_at_biggest_size(multi_context):
    """Every phase startup (get_ipns_csv) repoints the hardcoded core_info.csv at the
    biggest core_info_<size>.csv on disk — loop or not. Exercises the real method."""
    leaf = multi_context.sub_experiments[0]
    run = f"{leaf.get_experiment_folder_address()}/run"
    os.makedirs(run, exist_ok=True)
    for s, val in ((100, "1.0"), (450, "9.9")):
        with open(f"{run}/core_info_{s}.csv", "w") as f:
            f.write(f"ipns\n{val}\n")

    leaf.get_ipns_csv()

    assert open(leaf.run_core_info_path()).read() == "ipns\n9.9\n"


def test_result_command_generates_core_info_and_does_not_abort(multi_context):
    """RunResultCommand always generates core_info and never aborts on unmet bounds
    (so its cross-node save step always runs) — loop or not, same command."""
    leaf = multi_context.sub_experiments[0]
    cmd = " ".join(RunResultCommand(experiment_context=leaf).cmd())
    assert "--generate-core-info" in cmd
    assert "--no-exit-on-fail" in cmd


def test_save_off_by_default(multi_context):
    """save_next_core_info defaults False so a standalone/rerun result won't write a
    sized file by accident."""
    assert multi_context.save_next_core_info is False
    assert all(sub.save_next_core_info is False for sub in multi_context.sub_experiments)


def test_save_next_core_info_uses_max_across_nodes(multi_context):
    """The save step (when invoked) writes core_info_<next>.csv with next = max required
    across nodes, rounded to 50 — the cross-node step a single node can't do."""
    leaves = multi_context.sub_experiments
    for leaf, req in zip(leaves, (90, 410)):   # current sample_size is the default 30
        folder = leaf.get_experiment_folder_address()
        os.makedirs(f"{folder}/run", exist_ok=True)
        with open(f"{folder}/core_info_new.csv", "w") as f:
            f.write("ipns\n2.0\n")
        with open(f"{folder}/REQUIRED_SAMPLE_SIZE", "w") as f:
            f.write(str(req))

    RunResultCommand(experiment_context=multi_context)._save_next_core_info()

    # max(90, 410) = 410 -> ceil to 450, written on every node.
    for leaf in leaves:
        assert os.path.exists(f"{leaf.get_experiment_folder_address()}/run/core_info_450.csv")
