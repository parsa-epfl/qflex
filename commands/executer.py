import abc
import subprocess
import os


class Executor(abc.ABC):

    @abc.abstractmethod
    def cmd(self) -> str:
        pass

    def execute(self, to_stdio: bool = True, run_in_background: bool = False):
        args = self.cmd()
        cwd = os.getcwd()

        # TODO look into if shell needs to be turned False
        if run_in_background:
            # Background: optionally inherit stdio or capture, but you manage the pipes.
            return subprocess.Popen(
                args,
                shell=True,
                stdout=None if to_stdio else subprocess.PIPE,
                stderr=None if to_stdio else subprocess.PIPE,
                text=True,
                cwd=cwd,
            )

        # Foreground: safer to use subprocess.run (no deadlock). 
        if to_stdio:
            r = subprocess.run(
                args, 
                shell=True, 
                text=True,
                cwd=cwd,
            )
            return r
        else:
            r = subprocess.run(
                args,
                shell=True,
                text=True,
                capture_output=True,
                cwd=cwd,
            )
            return r
