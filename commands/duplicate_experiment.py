from commands import Executor
from commands.config import ExperimentContext
import os


class DuplicateExperiment(Executor):

    NEEDS_PDES_PEER_KILL = False

    # Contexts are intentionally NOT stored as `experiment_context`: this command
    # only copies folders, so the leaf prep / group fan-out must not kick in.
    def __init__(self,
                 destination: ExperimentContext,
                 source: ExperimentContext,
                 overwrite: bool = False,
                 replace: bool = False):
        self.destination = destination
        self.source = source
        self.overwrite = overwrite
        self.replace = replace

    def folder_pairs(self) -> list:
        src_nodes = [s.node_number for s in self.source.sub_experiments]
        dst_nodes = [s.node_number for s in self.destination.sub_experiments]
        if src_nodes != dst_nodes:
            raise ValueError(
                f"Experiment hierarchies differ: target nodes {src_nodes} vs config nodes {dst_nodes}."
            )
        pairs = [(self.source, self.destination)] + \
            list(zip(self.source.sub_experiments, self.destination.sub_experiments))
        return [(s.get_experiment_folder_address(), d.get_experiment_folder_address()) for s, d in pairs]

    def cmd(self) -> str:
        pairs = self.folder_pairs()
        missing = [src for src, _ in pairs if not os.path.exists(src)]
        if missing:
            raise FileNotFoundError(f"Target experiment folder(s) not found: {missing}")
        if not self.overwrite and not self.replace:
            existing = [dst for _, dst in pairs if os.path.exists(dst)]
            if existing:
                raise FileExistsError(
                    f"Destination experiment folder(s) already exist: {existing}. "
                    "Pass --overwrite or --replace."
                )
        cmds = []
        for src, dst in pairs:
            if self.replace:
                cmds.append(f'rm -rf {dst}')
            cmds.append(f"echo '{src} -> {dst}'")
            # Trailing slashes merge src's contents into dst (overwrite mode) instead of nesting.
            cmds.append(f'rsync -ah --info=progress2 {src}/ {dst}/')
        return cmds
