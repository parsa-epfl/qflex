import abc
import subprocess
import os
import time
from typing import List, Tuple
from multiprocessing import Pool, pool, pool


class Executor(abc.ABC):

    @abc.abstractmethod
    def cmd(self) -> str:
        pass

    def execute(self, to_stdio: bool = True, run_in_background: bool = False):
        args = self.cmd()
        cwd = os.getcwd()
        if isinstance(args, str):
            args = [args]

        arg = " && ".join([a.strip() for a in args])
        # TODO see if we need to support other type of concatting args

        # TODO look into if shell needs to be turned False
        if run_in_background:
            # Background: optionally inherit stdio or capture, but you manage the pipes.
            return subprocess.Popen(
                arg,
                shell=True,
                stdout=None if to_stdio else subprocess.PIPE,
                stderr=None if to_stdio else subprocess.PIPE,
                text=True,
                cwd=cwd,
            )

        # Foreground: safer to use subprocess.run (no deadlock). 
        if to_stdio:
            r = subprocess.run(
                arg, 
                shell=True, 
                text=True,
                cwd=cwd,
            )
            return r
        else:
            r = subprocess.run(
                arg,
                shell=True,
                text=True,
                capture_output=True,
                cwd=cwd,
            )
            return r
    
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

    def execute(self, to_stdio = True, run_in_background = False):
        # One by one execute the children and stop if any of them fails
        results = []
        for child in self.children:
            result = child.execute(to_stdio=to_stdio, run_in_background=run_in_background)
            results.append((child, result))
            if result.returncode != 0:
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
                raise RuntimeError(f"{child.__class__.__name__} failed with rc={result.returncode}\nstdout:\n{log}\nstderr:\n{err}")
        
        return results

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

    def execute(self, to_stdio: bool = False, run_in_background: bool = False):
        assert not run_in_background, "run_in_background is not supported for ParallelExecutor."

        results: list[subprocess.CompletedProcess] = []
        with Pool(processes=len(self.children)) as pool:
            async_results = [
                pool.apply_async(ParallelExecutor._execute_child, ((child, to_stdio),))
                for child in self.children
            ]

            while True:
                time.sleep(0.1)
                for i, r in enumerate(async_results):
                    if r.ready():
                        result: subprocess.CompletedProcess = r.get()
                        if result.returncode != 0:
                            pool.terminate()
                            raise RuntimeError(
                                f"{self.children[i].__class__.__name__} failed with rc={result.returncode}\n{result.stderr}"
                            )
                if all(r.ready() for r in async_results):
                    break

            results: list[subprocess.CompletedProcess] = [r.get() for r in async_results]

        return list(zip(self.children, results))


