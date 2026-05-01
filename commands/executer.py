import abc
import subprocess
import os
import time
from multiprocessing import Pool
from .config import ExperimentContext


DRY_RUN_ENV_VAR = "QFLEX_DRY_RUN"


def _is_dry_run(dry_run: bool) -> bool:
    """Honor the caller-passed flag; fall back to QFLEX_DRY_RUN env var."""
    if dry_run:
        return True
    return os.environ.get(DRY_RUN_ENV_VAR, "").lower() in ("1", "true", "yes")


class Executor(abc.ABC):

    @abc.abstractmethod
    def cmd(self) -> str:
        pass

    def get_experiment(self) -> ExperimentContext:
        if not hasattr(self, "experiment") or not isinstance(self.experiment, ExperimentContext):
            return None
        return self.experiment

    def execute(self, to_stdio: bool = True, run_in_background: bool = False, dry_run: bool = False) -> bool:
        args = self.cmd()
        cwd = os.getcwd()
        if isinstance(args, str):
            args = [args]

        arg = " && ".join([a.strip() for a in args])
        # TODO see if we need to support other type of concatting args

        if _is_dry_run(dry_run):
            print(f"[dry-run] {self.__class__.__name__} (cwd={cwd}):\n  {arg}")
            return True

        # TODO look into if shell needs to be turned False
        if run_in_background:
            raise NotImplementedError("run_in_background is not implemented yet.")
            # Not implemented due to cleanups not being implemented yet
            # Background: optionally inherit stdio or capture, but you manage the pipes.
            subprocess.Popen(
                arg,
                shell=True,
                stdout=None if to_stdio else subprocess.PIPE,
                stderr=None if to_stdio else subprocess.PIPE,
                text=True,
                cwd=cwd,
            )
            return True


        # Foreground: safer to use subprocess.run (no deadlock). 
        if to_stdio:
            r = subprocess.run(
                arg, 
                shell=True, 
                text=True,
                cwd=cwd,
            )
            self.clean_up()
            return r.returncode == 0
        else:
            r = subprocess.run(
                arg,
                shell=True,
                text=True,
                capture_output=True,
                cwd=cwd,
            )
            self.clean_up()
            return r.returncode == 0
    
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
        
class SequentialGroupExecutor(Executor):

    def __init__(self, children: list[Executor]):
        self.children = children

    def execute(self, to_stdio = True, run_in_background = False, dry_run: bool = False):
        # One by one execute the children and stop if any of them fails
        results = []
        for child in self.children:
            result = child.execute(to_stdio=to_stdio, run_in_background=run_in_background, dry_run=dry_run)
            results.append((child, result))
            if not result:
                # read ouptut and error for debugging
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

class ParallelExecutor(Executor):

    def cmd(self) -> str:
        raise NotImplementedError("ParallelExecutor call childrens execute instead.")

    def __init__(self, children: list[Executor]):
        self.children = children

    @staticmethod
    def _execute_child(args):
        child: Executor = args[0]
        to_stdio: bool = args[1]
        return child.execute(to_stdio=to_stdio, run_in_background=False)

    def execute(self, to_stdio: bool = False, run_in_background: bool = False, dry_run: bool = False):
        assert not run_in_background, "run_in_background is not supported for ParallelExecutor."

        if _is_dry_run(dry_run):
            for child in self.children:
                child.execute(to_stdio=to_stdio, dry_run=True)
            return True

        results: list[bool] = []
        with Pool(processes=len(self.children)) as pool:
            async_results = [
                pool.apply_async(ParallelExecutor._execute_child, ((child, to_stdio),))
                for child in self.children
            ]

            while True:
                time.sleep(0.1)
                for i, r in enumerate(async_results):
                    if r.ready():
                        result: bool = r.get()
                        if not result:
                            pool.terminate()
                            raise RuntimeError(
                                f"{self.children[i].__class__.__name__} failed."
                            )
                if all(r.ready() for r in async_results):
                    break

        self.clean_up()
        return True


