import glob
import os
import shutil
from .config import ExperimentContext
from .jinja_loaders import FlexusScriptLoader
from .partition import check_no_partition_running


def refresh_scripts(experiment_context: ExperimentContext):
    """
    Regenerates scripts/run_flexus.sh of an existing experiment from the current template, replaces the copies in
    its partitions, and adds the links to run/ files that newer versions of partition.py create. The configuration in cfg/ is left as it is: it is built into the WormCache plugin and the
    checkpoints, so changing it needs a new experiment.
    """
    folder = experiment_context.get_experiment_folder_address()
    script = f"{folder}/scripts/run_flexus.sh"
    if not os.path.isfile(script):
        raise FileNotFoundError(f"Error: {script} not found. Run initialize first.")
    check_no_partition_running(folder)

    with open(script) as f:
        old = f.read()
    FlexusScriptLoader(experiment_context=experiment_context).load_parameters()
    with open(script) as f:
        changed = f.read() != old
    print(f"{'Updated' if changed else 'Already up to date:'} {script}")

    partitions = sorted(glob.glob(f"{folder}/run/partition_*"))
    for partition in partitions:
        shutil.copy(script, f"{partition}/run_flexus.sh")
    if partitions:
        print(f"Copied it into {len(partitions)} partitions.")

    # Links added to partition.py after the experiment was partitioned.
    linked = 0
    for partition in partitions:
        for name in ["efi-virtio.rom"]:
            if os.path.exists(f"{folder}/run/{name}") and not os.path.lexists(f"{partition}/{name}"):
                os.symlink(f"../{name}", f"{partition}/{name}")
                linked += 1
    if linked:
        print(f"Added {linked} missing links to the files of run/ in the partitions.")
    # partition.py, run_partitions.sh, result.py, collect.py and the training scripts are copied from the
    # repository by every command (when the repository's copy is newer), so they are already current.
