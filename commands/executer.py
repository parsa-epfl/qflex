import abc
import subprocess
import os
from typing import List, Tuple


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
        
class ParallelExecutor(Executor):

    def cmd(self) -> str:
        raise NotImplementedError("ParallelExecutor call childrens execute instead.")

    def __init__(self, children: list[Executor]):
        self.children = children

    def execute(self, to_stdio: bool = False, run_in_background: bool = False):
        assert not run_in_background, "run_in_background is not supported for ParallelExecutor."
        processes: List[Tuple[Executor, subprocess.Popen]] = []
        for child in self.children:
            proc = child.execute(to_stdio=to_stdio, run_in_background=True)
            processes.append((child, proc))


        for _, proc in processes:
            proc.wait()
            print(f"Process for {proc.args} finished with return code {proc.returncode}.")

        failed = [(child, proc) for child, proc in processes if proc.returncode != 0]
        if failed:
            descriptions = [f"  {child.__class__.__name__} (rc={proc.returncode})" for child, proc in failed]
            raise RuntimeError(
                f"{len(failed)}/{len(processes)} parallel tasks failed:\n" + "\n".join(descriptions)
            )

        return processes
