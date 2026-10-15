"""
Prebuilt workloads: VM images whose workload already runs in steady state, so an experiment can
skip boot/load and start directly from `initialize` with `-loadvm base`.

The catalog is the S3 bucket listing at PREBUILT_URL. Each prebuilt workload is a folder
`workloads/<cores>/<workload>/` (e.g. `workloads/16c/dc/`) holding:
    config.json   machine settings the snapshot was taken with (cores, memory, network)
    root.qcow2    guest disk, with the disk snapshot entry "base"
    base.zstd     RAM/device state of the "base" snapshot (read by -loadvm base)
`./qflex image pull` downloads them to `<working-folder>/images/prebuilt/<cores>/<workload>/`; experiments
then copy root.qcow2 from there and link base.zstd (see ExperimentContext.set_up_prebuilt_image).
"""
import json
import os
import shutil
import urllib.error
import urllib.parse
import urllib.request
import xml.etree.ElementTree as ET
from dataclasses import dataclass, field

from rich.progress import BarColumn, DownloadColumn, Progress, TextColumn, TimeRemainingColumn, TransferSpeedColumn

PREBUILT_URL = os.environ.get("QFLEX_PREBUILT_URL", "https://datasets.epfl.ch/parsa-epfl")
PREBUILT_PREFIX = "workloads/"
# config.json is downloaded last: its presence marks a complete download.
PREBUILT_FILES = ["root.qcow2", "base.zstd", "config.json"]
S3_NS = "{http://s3.amazonaws.com/doc/2006-03-01/}"
CHUNK_SIZE = 8 * 1024 * 1024


@dataclass
class PrebuiltWorkload:
    id: str  # "<cores>/<workload>", e.g. "16c/dc"
    sizes: dict[str, int] = field(default_factory=dict)  # file name -> size in bytes

    @property
    def total_size(self) -> int:
        return sum(self.sizes.values())

    def url(self, file: str, base_url: str = PREBUILT_URL) -> str:
        return f"{base_url}/{urllib.parse.quote(PREBUILT_PREFIX + self.id + '/' + file)}"


def get_prebuilt_folder(image_folder: str, workload_id: str) -> str:
    return os.path.join(os.path.abspath(image_folder), "prebuilt", workload_id)


def is_downloaded(image_folder: str, workload_id: str) -> bool:
    return os.path.isfile(os.path.join(get_prebuilt_folder(image_folder, workload_id), "config.json"))


def fetch_catalog(base_url: str = PREBUILT_URL) -> list[PrebuiltWorkload]:
    """
    Lists the prebuilt workloads from the bucket listing (no per-workload requests).
    Only folders that contain all of PREBUILT_FILES are returned, sorted by core count then name.
    """
    workloads: dict[str, PrebuiltWorkload] = {}
    marker = ""
    while True:
        query = urllib.parse.urlencode({"prefix": PREBUILT_PREFIX, "marker": marker})
        with urllib.request.urlopen(f"{base_url}/?{query}", timeout=60) as response:
            root = ET.fromstring(response.read())
        keys = []
        for contents in root.iter(f"{S3_NS}Contents"):
            key = contents.findtext(f"{S3_NS}Key", "")
            keys.append(key)
            parts = key[len(PREBUILT_PREFIX):].split("/")
            if len(parts) != 3 or parts[2] not in PREBUILT_FILES:
                continue
            workload_id = f"{parts[0]}/{parts[1]}"
            workloads.setdefault(workload_id, PrebuiltWorkload(id=workload_id)).sizes[parts[2]] = int(contents.findtext(f"{S3_NS}Size", "0"))
        if root.findtext(f"{S3_NS}IsTruncated") != "true" or not keys:
            break
        marker = keys[-1]

    complete = [w for w in workloads.values() if set(w.sizes) == set(PREBUILT_FILES)]

    def sort_key(w: PrebuiltWorkload) -> tuple[int, str]:
        cores, workload = w.id.split("/")
        return (int(cores.rstrip("c")) if cores.rstrip("c").isdigit() else 0, workload)

    return sorted(complete, key=sort_key)


def find_workload(catalog: list[PrebuiltWorkload], workload_id: str) -> PrebuiltWorkload:
    for w in catalog:
        if w.id == workload_id:
            return w
    raise ValueError(f"Prebuilt workload '{workload_id}' not found. Run `./qflex image list` to see the available ones.")


def _download_file(url: str, target: str, size: int, progress: Progress) -> None:
    """Downloads url to target via target.part, resuming a previous partial download."""
    if os.path.isfile(target) and os.path.getsize(target) == size:
        progress.console.print(f"{os.path.basename(target)} already downloaded.")
        return
    part = target + ".part"
    offset = os.path.getsize(part) if os.path.isfile(part) else 0
    if offset > size:
        os.remove(part)
        offset = 0

    request = urllib.request.Request(url)
    if offset > 0:
        request.add_header("Range", f"bytes={offset}-")
    try:
        response = urllib.request.urlopen(request, timeout=60)
    except urllib.error.HTTPError as e:
        if e.code == 416 and offset == size:  # the .part file is already complete
            response = None
        else:
            body = e.read().decode(errors="replace")
            raise RuntimeError(f"Downloading {url} failed with HTTP {e.code}: {body[:300]}") from e

    task = progress.add_task(os.path.basename(target), total=size, completed=offset)
    if response is not None:
        with response:
            if offset > 0 and response.status != 206:
                # The server ignored the Range header; start over.
                offset = 0
                progress.update(task, completed=0)
            with open(part, "ab" if offset > 0 else "wb") as f:
                while chunk := response.read(CHUNK_SIZE):
                    f.write(chunk)
                    progress.advance(task, len(chunk))

    actual = os.path.getsize(part)
    if actual != size:
        raise RuntimeError(f"{part} has {actual} bytes, expected {size}. Run the pull again to resume.")
    os.chmod(part, 0o444)
    os.replace(part, target)


def pull(workload: PrebuiltWorkload, image_folder: str, base_url: str = PREBUILT_URL) -> str:
    """Downloads a prebuilt workload into <image_folder>/prebuilt/<id> and returns that folder."""
    folder = get_prebuilt_folder(image_folder, workload.id)
    os.makedirs(folder, exist_ok=True)

    missing = 0
    for file, size in workload.sizes.items():
        target = os.path.join(folder, file)
        if os.path.isfile(target) and os.path.getsize(target) == size:
            continue
        part = target + ".part"
        missing += size - (os.path.getsize(part) if os.path.isfile(part) else 0)
    free = shutil.disk_usage(folder).free
    if missing > free:
        raise RuntimeError(f"Not enough disk space in {folder}: need {missing / 1e9:.1f} GB, {free / 1e9:.1f} GB free.")

    with Progress(
        TextColumn("{task.description}"), BarColumn(), DownloadColumn(), TransferSpeedColumn(), TimeRemainingColumn(),
    ) as progress:
        for file in PREBUILT_FILES:
            _download_file(workload.url(file, base_url), os.path.join(folder, file), workload.sizes[file], progress)
    return folder


def load_prebuilt_config(folder: str) -> dict:
    with open(os.path.join(folder, "config.json")) as f:
        return json.load(f)


def parse_memory_gb(memory: str) -> int:
    """Converts a QEMU -m value such as "64G" (or "65536M") to GB."""
    memory = memory.strip().upper()
    units = {"G": 1, "M": 1 / 1024, "T": 1024}
    if memory[-1] in units:
        return int(float(memory[:-1]) * units[memory[-1]])
    return int(float(memory) / 1024)  # QEMU's default unit is MB
