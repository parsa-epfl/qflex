"""Progress reporting for the timing phase, driven by real completion events (no filesystem guessing).

A leaf reports `(node_number, partition_number, done, total)` up a `multiprocessing.Queue` via
`Executor._emit_progress`; the queue is set on the top-level command and inherited by forked children.
`progress_channel` (used by the top command) creates the queue and a renderer thread that drains it and
shows, per sub-experiment, a live tqdm bar on the tty plus that experiment's own `RunPartition.log`.
"""
import contextlib
import multiprocessing as mp
import os
import sys
import threading

from tqdm import tqdm


def node_spec(exp):
    """Render spec for one experiment context: {key, desc, folder} keyed by node_number."""
    folder = exp.get_experiment_folder_address()
    return {"key": exp.node_number, "desc": os.path.basename(folder.rstrip("/")), "folder": folder}


def _start_render(queue, nodes, tty):
    """nodes: list of {key, desc, folder}. Render per-node + TOTAL bars from queue events until stopped."""
    state = {}  # (node_key, partition) -> (done, total)
    files = {n["key"]: open(f"{n['folder']}/RunPartition.log", "w") for n in nodes}
    file_bars = {n["key"]: tqdm(desc="RunPartition", unit="idx", dynamic_ncols=True, file=files[n["key"]])
                 for n in nodes}
    tty_bars = {}
    if tty:
        for i, n in enumerate(nodes):
            tty_bars[n["key"]] = tqdm(desc=n["desc"], unit="idx", position=i, dynamic_ncols=True)
        tty_bars["_total"] = tqdm(desc="TOTAL", unit="idx", position=len(nodes), dynamic_ncols=True)

    def refresh():
        gdone = gtot = 0
        for n in nodes:
            done = tot = 0
            parts = []
            for (k, p), (d, t) in state.items():
                if k == n["key"]:
                    done += d
                    tot += t
                    parts.append((p, d, t))
            gdone += done
            gtot += tot
            postfix = " ".join(f"p{p}:{d}/{t}" for p, d, t in sorted(parts))
            for bar in (file_bars.get(n["key"]), tty_bars.get(n["key"])):
                if bar is None:
                    continue
                bar.total = max(tot, 1)
                bar.n = min(done, bar.total)
                bar.set_postfix_str(postfix, refresh=False)
                bar.refresh()
        if "_total" in tty_bars:
            b = tty_bars["_total"]
            b.total = max(gtot, 1)
            b.n = min(gdone, b.total)
            b.refresh()

    stop = threading.Event()

    def loop():
        while not stop.is_set():
            try:
                ev = queue.get(timeout=0.3)
            except Exception:
                ev = None
            dirty = False
            while ev is not None and ev != "STOP":
                k, p, d, t = ev
                state[(k, p)] = (d, t)
                dirty = True
                try:
                    ev = queue.get_nowait()
                except Exception:
                    ev = None
            if dirty:
                refresh()
            if ev == "STOP":
                break
        refresh()
        for bar in list(file_bars.values()) + list(tty_bars.values()):
            bar.close()
        for f in files.values():
            f.close()

    t = threading.Thread(target=loop, daemon=True)
    t.start()
    return stop, t


@contextlib.contextmanager
def progress_channel(owner, nodes, tty=True):
    """Top-level command opens this: create the queue (set on `owner`, inherited by forked children),
    start the renderer, and tear it down on exit. `nodes`: list of {key, desc, folder}."""
    queue = mp.Queue()
    owner._progress_queue = queue
    # tqdm bars are created here (calling thread) so they capture the real stderr before any child
    # process redirects its own — keeping the live bars on the terminal.
    stop, thread = _start_render(queue, nodes, tty and sys.stderr.isatty())
    try:
        yield
    finally:
        try:
            queue.put("STOP")
        except Exception:
            pass
        stop.set()
        thread.join(timeout=5)
