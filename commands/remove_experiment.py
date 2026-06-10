from commands import Executor
from commands.config import ExperimentContext
import os


class RemoveExperiment(Executor):

    NEEDS_PDES_PEER_KILL = False

    # Stored as `target`, NOT `experiment_context`, so the leaf prep / group fan-out don't kick in.
    def __init__(self, target: ExperimentContext):
        self.target = target

    def folders(self) -> list:
        ctxs = [self.target] + list(self.target.sub_experiments)
        return [c.get_experiment_folder_address() for c in ctxs]

    def cmd(self) -> str:
        folders = self.folders()
        existing = [f for f in folders if os.path.exists(f)]
        if not existing:
            raise FileNotFoundError(f"No experiment folder(s) to remove among: {folders}")
        cmds = []
        for f in existing:
            cmds.append(f"echo 'removing {f}'")
            cmds.append(f'rm -rf {f}')
        cmds.append("echo removed experiment successfully")
        return cmds
