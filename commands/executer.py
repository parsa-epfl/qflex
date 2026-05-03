import abc
import multiprocessing as mp
import os
import shutil
import subprocess
import time
from .config import ExperimentContext


DRY_RUN_ENV_VAR = "QFLEX_DRY_RUN"


def _is_dry_run(dry_run: bool) -> bool:
    """Honor the caller-passed flag; fall back to QFLEX_DRY_RUN env var."""
    if dry_run:
        return True
    return os.environ.get(DRY_RUN_ENV_VAR, "").lower() in ("1", "true", "yes")


KILLED_BY_PEER_SUFFIX = "killed_by_peer"


def _multi_experiment_target(executor: "Executor", sub: ExperimentContext, kw: dict) -> None:
    """mp.Process target. Module-level so it's picklable on all platforms.

    Mutates the (pickled) executor's experiment_context to point at one sub-experiment,
    then runs its execute() recursively. Each child process gets its own copy via
    the multiprocessing fork/spawn — the parent's instance is untouched."""
    executor.experiment_context = sub
    try:
        ok = bool(executor.execute(**kw))
    except BaseException as exc:
        print(f"[multi-experiment] child {executor.__class__.__name__} raised: {exc}")
        os._exit(1)
    os._exit(0 if ok else 1)


class Executor(abc.ABC):
    # Class-level opt-in for the interactive-tmux dispatch path. False on the
    # base class so phases like fw / run-partition ignore the field even if a
    # context with `interactive_tmux=True` flows into them. Boot and Load set
    # this to True.
    SUPPORTS_INTERACTIVE: bool = False

    @abc.abstractmethod
    def cmd(self) -> str:
        pass

    def get_experiment(self) -> ExperimentContext:
        # Prefer the longer attribute name used by every concrete command;
        # fall back to the legacy `experiment` attribute for any callers that still set it.
        for attr in ("experiment_context", "experiment"):
            if hasattr(self, attr):
                value = getattr(self, attr)
                if isinstance(value, ExperimentContext):
                    return value
        return None

    def _phase_name(self) -> str:
        """Used in sentinel filenames. Default: class name."""
        return self.__class__.__name__

    # Multi-node post-exit grace. After this leaf's bash returns, peer leaves'
    # qemus may still be finishing their per-node savevm or running quit_qemu —
    # so wait before _kill_peer_qemus pkills them. Single-node skips entirely.
    # TODO replace this fixed sleep with a deterministic per-leaf "i'm done"
    # sentinel and have peers wait on each other via that. The 30s heuristic
    # is here so post-savevm finalisation has time without an extra handshake.
    POST_EXIT_GRACE_SECONDS: int = 30

    def _post_exit_grace(self, exp: ExperimentContext, returncode: int) -> None:
        if not exp.is_multi_node():
            return
        seconds = self.POST_EXIT_GRACE_SECONDS
        print(
            f"[executor] {self.__class__.__name__} node {exp.node_number}: "
            f"bash exited rc={returncode}; waiting {seconds}s grace before peer "
            f"cleanup so neighbours can finish savevm / quit_qemu cleanly.",
            flush=True,
        )
        time.sleep(seconds)

    def _kill_peer_qemus(self, sentinel_dir: str) -> None:
        # TODO multi-node teardown bug: parallel-qemu's PDES exit handling leaves
        # peer nodes hanging after one node quits. Fix in
        # parallel-qemu/net/pdes-engine.c so this workaround can be removed.
        # Symmetric — every leaf (not just master) pkills its peers on exit so
        # neither side gets stuck waiting for the other in PDES sync.
        exp = self.get_experiment()
        if exp is None or not exp.neighbor_node_list:
            return
        partition_str = f"part_{exp.partition_number}_" if exp.partition_number >= 0 else ""
        idx_str = f"idx_{exp.idx}_" if exp.idx >= 0 else ""
        for neighbor in exp.neighbor_node_list:
            shm = f"pdes_{neighbor}_to_{exp.node_number}{partition_str}{idx_str}"
            subprocess.run(["pkill", "-9", "-f", f"shm-send=/{shm}"], check=False)
            if sentinel_dir is not None:
                os.makedirs(sentinel_dir, exist_ok=True)
                path = f"{sentinel_dir}/{self._sentinel_basename(neighbor)}.{KILLED_BY_PEER_SUFFIX}"
                open(path, "w").close()

    def _sentinel_basename(self, node_number: int) -> str:
        """Sentinel basename includes partition_number and idx when set, so RunIdxCommand
        instances coordinate per-(partition, idx) pair across nodes — matching the
        granularity of the multi-node shm rings (named with part/idx suffixes too)."""
        exp = self.get_experiment()
        parts = [self._phase_name()]
        if exp is not None and exp.partition_number >= 0:
            parts.append(f"part{exp.partition_number}")
        if exp is not None and exp.idx >= 0:
            parts.append(f"idx{exp.idx}")
        parts.append(f"node{node_number}")
        return "_".join(parts)

    # ----- top-level dispatch -----

    def execute(self,
                to_stdio: bool = True,
                run_in_background: bool = False,
                dry_run: bool = False,
                *,
                sentinel_dir: str = None,
                log_path: str = None,
                err_path: str = None) -> bool:
        exp = self.get_experiment()
        if exp is not None and exp.has_sub_experiments():
            return self._execute_group(to_stdio=to_stdio,
                                       run_in_background=run_in_background,
                                       dry_run=dry_run,
                                       outer_sentinel_dir=sentinel_dir)
        return self._execute_leaf(to_stdio=to_stdio,
                                  run_in_background=run_in_background,
                                  dry_run=dry_run,
                                  sentinel_dir=sentinel_dir,
                                  log_path=log_path,
                                  err_path=err_path)

    def _execute_group(self, to_stdio: bool, run_in_background: bool, dry_run: bool,
                       outer_sentinel_dir: str = None) -> bool:
        """Dispatch the same logical phase across self.experiment_context.sub_experiments.
        Each sub runs in its own mp.Process; the child mutates self.experiment_context = sub
        and re-enters execute() — which sees a leaf context and runs the leaf path.

        This is the universal "another axis of parallelism" mechanism: it replaces the
        former ParallelExecutor. Multi-node, partitions, or any other tree level uses
        the same code path. Sub-experiments live on ExperimentContext (statically from
        YAML for nodes, dynamically generated by executors like RunPartitionCommand for
        partitions). The executor's per-command flags travel via pickle — no kwargs
        stashing, no executor cloning."""
        assert not run_in_background, "run_in_background is not supported for group dispatch."
        exp = self.get_experiment()
        phase = self._phase_name()
        # Sentinel directory: nested groups inherit the outermost group's sentinel dir
        # so leaves at any depth coordinate against the same set of files. The outermost
        # call (no outer_sentinel_dir) initialises a fresh dir under the group's folder.
        if outer_sentinel_dir is None:
            sentinel_dir = f"{exp.get_experiment_folder_address()}/.sentinels"
            sentinel_dir_owner = True
        else:
            sentinel_dir = outer_sentinel_dir
            sentinel_dir_owner = False

        def per_child_kwargs(sub: ExperimentContext) -> dict:
            sub_folder = sub.get_experiment_folder_address()
            return dict(
                to_stdio=False,
                run_in_background=False,
                sentinel_dir=sentinel_dir,
                log_path=f"{sub_folder}/{phase}.log",
                err_path=f"{sub_folder}/{phase}.err",
            )

        if _is_dry_run(dry_run):
            for sub in exp.sub_experiments:
                # Mutate locally for dry-run print (we're in the parent process, but no
                # mp.Process spawn). Restore after each iteration.
                original = self.experiment_context
                self.experiment_context = sub
                try:
                    self.execute(**per_child_kwargs(sub), dry_run=True)
                finally:
                    self.experiment_context = original
            return True

        # The outermost group cleans stale sentinels exactly once per phase invocation.
        # Nested groups must NOT wipe — they share the parent's sentinel dir.
        if sentinel_dir_owner:
            shutil.rmtree(sentinel_dir, ignore_errors=True)
            os.makedirs(sentinel_dir, exist_ok=True)

        procs = []
        for sub in exp.sub_experiments:
            kw = per_child_kwargs(sub)
            p = mp.Process(target=_multi_experiment_target, args=(self, sub, kw))
            procs.append((sub, p))
            p.start()

        failures = []
        for sub, p in procs:
            p.join()
            if p.exitcode == 0:
                continue
            killed_marker = f"{sentinel_dir}/{self._sentinel_basename(sub.node_number)}.{KILLED_BY_PEER_SUFFIX}"
            if os.path.exists(killed_marker):
                continue
            failures.append((sub, p.exitcode))

        if failures:
            names = ", ".join(
                f"node_{s.node_number}(exit={c})" for s, c in failures
            )
            raise RuntimeError(
                f"{self.__class__.__name__}: sub-experiments failed: {names}"
            )
        return True

    def _sentinel_path(self, sentinel_dir: str, node_number: int, suffix: str) -> str:
        return f"{sentinel_dir}/{self._sentinel_basename(node_number)}.{suffix}"

    def _wait_for_sentinels(self, sentinel_dir: str) -> None:
        """Block (in Python) until every node listed in self.experiment_context.wait_for_nodes
        has touched its <basename>.started sentinel. Cleaner than spinning in bash, and the
        polling loop stays in the Python process the executor is running in."""
        if sentinel_dir is None:
            return
        exp = self.get_experiment()
        if exp is None or not exp.wait_for_nodes:
            return
        for n in exp.wait_for_nodes:
            path = self._sentinel_path(sentinel_dir, n, "started")
            while not os.path.exists(path):
                time.sleep(0.5)

    def _touch_sentinel(self, sentinel_dir: str, suffix: str) -> None:
        if sentinel_dir is None:
            return
        exp = self.get_experiment()
        if exp is None:
            return
        path = self._sentinel_path(sentinel_dir, exp.node_number, suffix)
        os.makedirs(os.path.dirname(path), exist_ok=True)
        open(path, "w").close()

    def _build_bash(self, log_path: str, err_path: str, *, tee_to_stdio: bool = False) -> str:
        """Join self.cmd() into a single bash string, optionally wrapping with an
        output redirect. The sentinel wait/touch is done in Python (see _execute_leaf),
        not embedded here. tee_to_stdio=True keeps the live terminal stream while
        also persisting to file — needed when the leaf runs foreground so the user
        sees QEMU progress and the log survives if the run dies. Uses bash process
        substitution; the leaf invokes via `bash -c` (not /bin/sh) to make it work."""
        args = self.cmd()
        if isinstance(args, str):
            args = [args]
        # TODO see if we need to support other type of concatting args
        inner = " && ".join(a.strip() for a in args)
        if not (log_path and err_path):
            return inner
        if tee_to_stdio:
            return f"( {inner} ) > >(tee {log_path}) 2> >(tee {err_path} >&2)"
        return f"( {inner} ) > {log_path} 2> {err_path}"

    def _execute_leaf(self,
                      to_stdio: bool,
                      run_in_background: bool,
                      dry_run: bool,
                      sentinel_dir: str,
                      log_path: str,
                      err_path: str) -> bool:
        cwd = os.getcwd()
        exp = self.get_experiment()

        # Always persist QEMU stdout/stderr under the experiment folder so the
        # user can inspect a failed run after the fact (expect dying mid-script,
        # gdb crash, …). Skipped only for the interactive_tmux path, which gets
        # an exclusive `if` branch below — there the user reads output live in
        # the pane, so a separate file would be redundant.
        if exp is not None and log_path is None:
            log_path = f"{exp.get_experiment_folder_address()}/{self._phase_name()}.log"
        if exp is not None and err_path is None:
            err_path = f"{exp.get_experiment_folder_address()}/{self._phase_name()}.err"

        if _is_dry_run(dry_run):
            # Run the pure (no-fs) part of leaf prep so the printed bash reflects
            # auto-computed ports / auto-flipped flags. The fs side effects
            # (set_up_folders, shm cleanup, qemu_nic mutation) are still skipped.
            if exp is not None:
                exp.compute_runtime_settings()
            self._print_dry_run_actions(cwd, sentinel_dir, log_path, err_path,
                                        to_stdio=to_stdio)
            return True

        if run_in_background:
            raise NotImplementedError("run_in_background is not implemented yet.")

        # Path B (interactive_tmux): only available on Boot/Load (gated by
        # SUPPORTS_INTERACTIVE). Sends the bash to a fresh tmux window per leaf,
        # then blocks until the user has actually quit QEMU in that window.
        if (self.SUPPORTS_INTERACTIVE
                and exp is not None
                and exp.interactive_tmux):
            return self._execute_in_tmux(sentinel_dir, log_path, err_path)

        # Leaf-level filesystem prep first — moved out of create_experiment_context
        # so DI graph construction has no side effects. Both this leaf and any
        # upstream leaves prep in parallel; we don't wait for upstream until prep
        # is done. Otherwise multi-node QEMU's PDES sync would deadlock the master
        # while a follower is still cping its per-node qcow2 (~tens of seconds).
        if exp is not None:
            exp.prepare_for_execution()

        # NOW block until upstream nodes have actually launched their phase. The
        # `started` sentinel signifies "I've finished prep and am about to spawn
        # QEMU" — that's the right moment for downstream nodes to also spawn.
        self._wait_for_sentinels(sentinel_dir)

        # Mark this leaf as started before the bash runs, so any node waiting on us
        # (its wait_for_nodes contains our node_number) can proceed.
        self._touch_sentinel(sentinel_dir, "started")

        arg = self._build_bash(log_path, err_path, tee_to_stdio=to_stdio)

        # Force bash (not /bin/sh / dash) so process substitution `>(tee ...)` works.
        if to_stdio:
            r = subprocess.run(["bash", "-c", arg], text=True, cwd=cwd)
        else:
            r = subprocess.run(["bash", "-c", arg], text=True, capture_output=True, cwd=cwd)
        if exp is not None:
            self._post_exit_grace(exp, r.returncode)
            self._kill_peer_qemus(sentinel_dir)
        self.clean_up()

        if r.returncode == 0:
            self._touch_sentinel(sentinel_dir, "done")
            return True
        return False

    def _execute_in_tmux(self, sentinel_dir: str, log_path: str, err_path: str) -> bool:
        """Path B: open one tmux window per leaf and send the bash to it. The user
        attaches manually and quits QEMU when finished; we poll for the .done
        sentinel that the bash itself touches at the end."""
        # libtmux is an optional dependency; lazy-import so non-interactive runs
        # don't pay the cost or require the package.
        import libtmux  # type: ignore

        exp = self.get_experiment()

        # Open the window FIRST so the user sees a pane per leaf immediately —
        # otherwise prepare_for_execution()'s file copies (~tens of seconds on
        # first run) make non-master nodes look like they "didn't launch tmux".
        server = libtmux.Server()
        if not server.sessions:
            raise RuntimeError(
                "interactive_tmux requires a running tmux server — "
                "run `tmux new -s qflex` first, then re-run."
            )
        target_session_id = os.environ.get("TMUX")
        session = None
        if target_session_id:
            for s in server.sessions:
                if s.session_id and s.session_id in target_session_id:
                    session = s
                    break
        if session is None:
            session = server.sessions[0]

        window_name = f"{self._phase_name()}-node{exp.node_number}" if exp else self._phase_name()
        window = session.new_window(window_name=window_name, attach=False)
        pane = window.panes[0]
        node_str = f"node {exp.node_number}" if exp is not None else "leaf"
        pane.send_keys(
            f"echo '[qflex] {node_str}: preparing experiment files (this can take ~tens of seconds on first run)...'",
            enter=True,
        )

        # Same prep-then-wait dance as the subprocess path: prep first (both
        # nodes can do it in parallel), then wait for upstream's started, then
        # touch our own started.
        if exp is not None:
            exp.prepare_for_execution()
        self._wait_for_sentinels(sentinel_dir)
        self._touch_sentinel(sentinel_dir, "started")

        # Append the .done sentinel touch so we know exactly when QEMU exits.
        # _build_bash emits the unredirected bash here — output stays in the
        # pane buffer for the user to read live.
        bash = self._build_bash(None, None)
        done_marker = self._sentinel_path(sentinel_dir, exp.node_number, "done") if exp else None
        if done_marker:
            bash = f"{bash}; touch {done_marker}"
        pane.send_keys(bash, enter=True)

        # Block until the user has finished interacting and QEMU exited.
        if done_marker:
            while not os.path.exists(done_marker):
                time.sleep(0.5)

        self.clean_up()
        return True

    def _print_dry_run_actions(self, cwd: str, sentinel_dir: str,
                               log_path: str, err_path: str,
                               *, to_stdio: bool = False) -> None:
        """Print the full sequence (Python wait/touch + bash + done-touch) so dry-run
        output reflects what _execute_leaf would actually do at run time. Embedded
        newlines in the bash (e.g. gdb's multiline python blocks) are collapsed to '\\n'
        so each marker stays on one line — both for human readability and for tooling
        that parses dry-run output line-by-line."""
        lines = [f"[dry-run] {self.__class__.__name__} (cwd={cwd}):"]
        exp = self.get_experiment()
        in_tmux_mode = (
            self.SUPPORTS_INTERACTIVE
            and exp is not None
            and exp.interactive_tmux
        )
        # Tmux mode opens the window + prints a "preparing..." echo *before*
        # prepare_for_execution, so the user sees a pane per leaf immediately.
        if in_tmux_mode:
            window_name = f"{self._phase_name()}-node{exp.node_number}"
            node_str = f"node {exp.node_number}"
            lines.append(f"  [tmux]     would open new window '{window_name}' and send-keys the bash")
            lines.append(f"  [bash]     echo '[qflex] {node_str}: preparing experiment files (this can take ~tens of seconds on first run)...'")
        if sentinel_dir is not None and exp is not None:
            for n in exp.wait_for_nodes:
                lines.append(f"  [py-wait]  {self._sentinel_path(sentinel_dir, n, 'started')}")
            lines.append(f"  [py-touch] {self._sentinel_path(sentinel_dir, exp.node_number, 'started')}")
        # Tmux mode runs unredirected bash inside the pane; subprocess mode applies log/err redirect.
        bash_log = None if in_tmux_mode else log_path
        bash_err = None if in_tmux_mode else err_path
        bash = self._build_bash(bash_log, bash_err,
                                tee_to_stdio=(to_stdio and not in_tmux_mode)
                                ).replace("\n", "\\n")
        if in_tmux_mode and sentinel_dir is not None and exp is not None:
            # Mirror the runtime: the tmux dispatch appends a `; touch <done>` to
            # the bash so Python can poll for QEMU exit.
            done_marker = self._sentinel_path(sentinel_dir, exp.node_number, 'done')
            bash = f"{bash}; touch {done_marker}"
        lines.append(f"  [bash]     {bash}")
        if in_tmux_mode and sentinel_dir is not None and exp is not None:
            lines.append(f"  [py-poll]  {self._sentinel_path(sentinel_dir, exp.node_number, 'done')}")
        elif sentinel_dir is not None and exp is not None:
            lines.append(f"  [py-touch] {self._sentinel_path(sentinel_dir, exp.node_number, 'done')}")
        print("\n".join(lines))

    def clean_up(self):
        experiment = self.get_experiment()
        if experiment is not None:
            experiment.clean_up()
        return

    def get_log_file_address(self):
        raise NotImplementedError("log_file_address is not implemented for this executor.")
    def get_err_file_address(self):
        raise NotImplementedError("err_file_address is not implemented for this executor.")


class SimpleCMDExecutor(Executor):

    def __init__(self, command: str):
        self.command = command

    def cmd(self) -> str:
        return self.command


class SimulationCommand(Executor):
    """Base for phases that drive a multi-node simulation (FW, InitWarm, Load,
    run-partition / run-single-partition / run-idx). Boot and image-creation
    are NOT simulation phases — they shouldn't inherit from this. The shared
    invariant: every link in `syncs_list` must be `"true"` so per-node sampling
    stays aligned. Single-node (empty syncs_list) is vacuously fine."""

    def _assert_syncs_true(self) -> None:
        exp = self.get_experiment()
        if exp is None or not exp.syncs_list:
            return
        bad = [s for s in exp.syncs_list if s != "true"]
        assert not bad, (
            f"{self.__class__.__name__}: all syncs_list entries must be 'true' "
            f"(this phase needs deterministic multi-node sync), got {exp.syncs_list}"
        )


class SequentialGroupExecutor(Executor):
    """Sequential dispatch over a heterogeneous list of children — used when the steps
    aren't 1:1 with experiments (e.g. RunSinglePartitionCommand interleaves RunIdxCommand
    with SimpleCMDExecutor sleeps). For homogeneous parallelism over experiments, prefer
    setting sub_experiments on the context and letting the base Executor.execute() dispatch."""

    def __init__(self, children: list[Executor]):
        self.children = children

    def execute(self, to_stdio = True, run_in_background = False, dry_run: bool = False,
                *, sentinel_dir: str = None, log_path: str = None, err_path: str = None):
        # If this orchestrator's experiment_context has sub-experiments, recurse via the
        # multi-experiment dispatch (the same mechanism that replaced ParallelExecutor).
        # Each sub gets its own mp.Process; in the child, the heterogeneous children
        # list is built against the sub's context.
        exp = self.get_experiment()
        if exp is not None and exp.has_sub_experiments():
            return self._execute_group(to_stdio=to_stdio,
                                       run_in_background=run_in_background,
                                       dry_run=dry_run,
                                       outer_sentinel_dir=sentinel_dir)

        # Subclasses (e.g. RunSinglePartitionCommand) build self.children lazily by
        # overriding _build_children() — empty default for the base class.
        self.children = self._build_children()

        # One by one execute the children and stop if any of them fails.
        # Sentinel_dir/log_path/err_path are forwarded so leaves down the tree
        # (e.g. RunIdxCommand) can do their own per-(partition,idx) sentinel touch.
        for child in self.children:
            result = child.execute(to_stdio=to_stdio,
                                   run_in_background=run_in_background,
                                   dry_run=dry_run,
                                   sentinel_dir=sentinel_dir,
                                   log_path=log_path,
                                   err_path=err_path)
            if not result:
                # read output and error for debugging
                err_f = child.get_err_file_address()
                log_f = child.get_log_file_address()
                err = ""
                log = ""
                if err_f is not None and os.path.exists(err_f):
                    with open(err_f, "r") as f:
                        err = f.read()
                if log_f is not None and os.path.exists(log_f):
                    with open(log_f, "r") as f:
                        log = f.read()
                raise RuntimeError(f"{child.__class__.__name__} failed \nstdout:\n{log}\nstderr:\n{err}")
        self.clean_up()
        return True

    def _build_children(self) -> list[Executor]:
        """Subclasses override to build children lazily from self.experiment_context.
        Default returns whatever was passed to __init__."""
        return self.children
