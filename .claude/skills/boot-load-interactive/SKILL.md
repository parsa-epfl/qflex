---
name: boot-load-interactive
description: Use when working on the two opt-in interactive paths for `boot` and `load` — Path A (`interaction_script` running an `expect`/bash/python script alongside QEMU, talking to it over telnet serial + telnet monitor) and Path B (`interactive_tmux` opening one tmux window per leaf via libtmux). Covers the QEMU host-facing channels (serial console vs HMP monitor vs guest agent) and how each path wires them up, the `compute_runtime_settings` auto-port machinery (`telnet_port = 55558+N`, `serial_telnet_port = 55600+N`), the env vars the user's script gets (`TELNET_SERIAL_PORT`, `TELNET_MONITOR_PORT`, `SERIAL_LOG_PATH`, `EXP_FOLDER`, `NODE_NUMBER`), the `SUPPORTS_INTERACTIVE` class-level gate on `Boot`/`Load`, the `_execute_in_tmux` poll-on-done flow, and the dry-run marker conventions for both paths. TRIGGER when the user mentions interaction_script / interactive_tmux / libtmux / expect / tmux window per leaf / telnet serial vs telnet monitor / qemu-guest-agent / sendkey / `mon:stdio` Ctrl-A C, or asks how to drive QEMU non-interactively under multi-node, or how to attach to per-node tmux panes. SKIP for the underlying executor dispatch / sentinels (executor skill), the YAML/DI wiring of these fields (qflex-dependency-injection skill), or unrelated multi-node PDES details (multi-node skill).
---

# Boot/Load interactive paths — scripted (Path A) and tmux (Path B)

`boot` and `load` are the only two phases where the user typically wants to actually *interact* with QEMU — install packages, partition disks, configure the workload, `savevm base`, etc. Once multi-node dispatch redirects each leaf's stdout/stderr to log files, the user can no longer just type at the QEMU monitor. Two opt-in per-leaf modes solve this. Both are gated by the class-level flag `SUPPORTS_INTERACTIVE = True` on `Boot` and `Load` only — every other phase ignores the relevant fields.

## QEMU has three host-facing channels (mental model)

This is the part most people get wrong. Don't conflate these:

| Channel | What it does | How QEMU exposes it | What it CAN'T do |
|---|---|---|---|
| **Serial console** | The guest OS's `tty`. Login prompt, shell, `dmesg`, `apk add`, your shell pipeline output. Bidirectional. | `-serial <chardev>` — common targets: `mon:stdio`, `file:serial.log`, `telnet:127.0.0.1:<port>,server,nowait`, a Unix socket. | Cannot pause/savevm the VM. It's just the guest's stdin/stdout. |
| **HMP / QMP monitor** | The *VM-level* control surface of QEMU itself: `savevm`, `loadvm`, `quit`, `info status`, `cont`, `stop`, `sendkey`. Operates on the VM, not the guest. | `-monitor <chardev>`. Today's `use_telnet_monitor=True` ([commands/qemu.py:72-74](../../../commands/qemu.py#L72)) puts it on `telnet:127.0.0.1:<telnet_port>`. | Cannot run a guest command like `apt-get install foo`. The monitor sees QEMU's perspective, not the guest OS. (`sendkey` exists but typing entire installs as keystrokes is impractical.) |
| **Guest agent** | The guest's *agent* (`qemu-guest-agent`) speaks structured QMP back to the host: `guest-exec`, `guest-file-write`, `guest-fsfreeze`, etc. | `-chardev socket,...,id=qga0 -device virtio-serial -device virtserialport,chardev=qga0,name=org.qemu.guest_agent.0`. Requires `qemu-guest-agent` installed *and running* in the guest. | Useless before the guest is set up the first time — there's no agent yet. So it can't bootstrap a brand-new VM. |

**Implications:**

- The `mon:stdio` shorthand multiplexes monitor + serial on one chardev (Ctrl-A C to switch). That's fine for a human at a tmux pane (Path B). It's **bad for `expect`** (Path A): expect against a multiplexed stream has to deal with the Ctrl-A C escape and can't reliably distinguish monitor vs guest output. So under Path A we keep them on **separate** telnet ports.
- For the *first ever* boot of an image the guest agent isn't installed yet, so we can't rely on it. The Path A bootstrap is "drive the serial console with `expect`": wait for `login:`, send `root\n`, etc. After installing `qemu-guest-agent` inside the guest and `savevm base`-ing, future `load` runs could additionally use it (not implemented in v1).

## Per-leaf YAML / context fields

Three leaf-level fields on [`ExperimentContext`](../../../commands/config.py) drive these paths:

```python
interaction_script: str = ""           # Path A: path to a script that drives QEMU
interactive_tmux: bool = False         # Path B: open tmux window per leaf
serial_telnet_port: int = -1           # auto = 55600 + node_number when -1
```

(Plus the existing `use_telnet_monitor: bool` and `telnet_port: int` — auto = `55558 + node_number`.)

`ExperimentContext.compute_runtime_settings()` ([commands/config.py](../../../commands/config.py)) — the **pure** part of leaf prep, called by both `_execute_leaf` (real run) and the dry-run path so rendered bash reflects the auto-computed values:

- If `interaction_script` is set and `use_telnet_monitor` is False, flips `use_telnet_monitor=True` (with a one-line print). Path A always needs telnet monitor.
- If `use_telnet_monitor`: auto-computes `telnet_port = 55558 + node_number` (when `-1`).
- If `interaction_script`: auto-computes `serial_telnet_port = 55600 + node_number` (when `-1`).

The two ports use different bases (`55558` vs `55600`) so they never collide.

## Path A — scripted automation

```
            ./qflex boot -c <multi.yaml>     # YAML has interaction_script: ./drive.exp per leaf
                       │
                       ▼  multi-experiment dispatch
              one mp.Process per leaf
                       │
                       ▼  Boot.cmd() builds the bash:
              cd <run-folder>
              { TELNET_SERIAL_PORT=55600 TELNET_MONITOR_PORT=55558 SERIAL_LOG_PATH=./serial.log
                EXP_FOLDER=… NODE_NUMBER=0 ./drive.exp & SCRIPT_PID=$!
                gdb -ex run --args ./qemu-system-aarch64 …    \
                                     -monitor telnet:127.0.0.1:55558,server,nowait \
                                     -serial  telnet:127.0.0.1:55600,server,nowait
                wait $SCRIPT_PID 2>/dev/null
              }
```

The script is backgrounded (`&`); QEMU runs in the foreground. When QEMU exits (because the script issued `quit` over the monitor, or the user's script crashed), the leaf is done. Sentinels are touched by Python the same way the standard `_execute_leaf` flow does.

The script gets these env vars:

| Var | Use |
|---|---|
| `TELNET_SERIAL_PORT` | Connect via `spawn telnet 127.0.0.1 $env(TELNET_SERIAL_PORT)` for guest console: login, shell, send commands. |
| `TELNET_MONITOR_PORT` | Connect for HMP commands: `savevm`, `quit`. Don't multiplex with serial. |
| `SERIAL_LOG_PATH` | Path to `./serial.log` if the script prefers polling a file over telnet. |
| `EXP_FOLDER` | Per-leaf experiment folder (so the script can reach `cfg/`, `run/`, etc.). |
| `NODE_NUMBER` | `0` for master, `1` for non-master, … Useful for branching. |

The leaf bash structure is built in [`Boot.cmd()`](../../../commands/boot.py) and [`Load.cmd()`](../../../commands/load.py) — they check `experiment_context.interaction_script` and switch from the default `[cd, gdb …]` list to a single-string compound command using `{ … ; }` shell grouping (so cwd persists past the `cd`).

`Boot.cmd()` / `Load.cmd()` also pass `use_stdio=False` to `QemuCommonArgParser` when `interaction_script` is set; combined with `use_telnet_monitor=True`, [`get_stdio()`](../../../commands/qemu.py) emits `-serial telnet:127.0.0.1:<serial_port>,server,nowait` (not `-serial mon:stdio`). It also suppresses `-monitor none` when monitor is on telnet (avoids a conflict with `-monitor telnet:...`).

Pseudo-script the user might write (illustrative — we don't ship one; we just expose env vars):

```tcl
#!/usr/bin/env expect
set serial_port $env(TELNET_SERIAL_PORT)
set monitor_port $env(TELNET_MONITOR_PORT)

spawn telnet 127.0.0.1 $serial_port
expect "login:"          ; send "root\r"
expect "# "              ; send "apk add curl …\r"
expect "# "
close

spawn telnet 127.0.0.1 $monitor_port
expect "(qemu) "         ; send "savevm base\r"
expect "(qemu) "         ; send "quit\r"
```

`expect` and `telnet` are already installed in the dev Docker image (commit 6c2dce6).

## Path B — tmux interactive windows

```
            ./qflex boot -c <multi.yaml>     # YAML has interactive_tmux: true per leaf
                       │
                       ▼  multi-experiment dispatch
              one mp.Process per leaf
                       │
                       ▼  Executor._execute_in_tmux:
              libtmux.Server() → existing session (no auto-start)
              session.new_window(name="Boot-node0", attach=False)
              pane.send_keys("<bash>; touch <done-marker>", enter=True)
              # Python BLOCKS polling for <done-marker> to appear
```

`_execute_in_tmux` ([commands/executer.py](../../../commands/executer.py)) is invoked at the top of `_execute_leaf` when **all three** conditions hold:

1. `self.SUPPORTS_INTERACTIVE` is `True` (set on `Boot` and `Load` only).
2. `experiment_context.interactive_tmux` is `True`.
3. We're not in dry-run.

The bash sent to the pane is whatever `cmd()` produces — for Path B we keep the **default** QEMU args (`-serial mon:stdio`, no auto-flip of `use_telnet_monitor`). Stdio multiplexed monitor + serial is exactly what the user wants when typing in a pane: Ctrl-A C switches between guest console and monitor for `savevm`. No `expect` involvement.

The trailing `; touch <done-marker>` is appended by `_execute_in_tmux` itself. The marker path is `<sentinel_dir>/<basename>.done` — same file the standard subprocess flow would touch. Python polls `os.path.exists(done_marker)` in a 0.5-second loop until the bash inside tmux finishes (i.e. the user has quit QEMU). Only then does the leaf return success.

Multi-node coordination: master's `.started` sentinel is touched right after `send_keys` (so non-master can proceed), and the master can be configured / quit at the user's pace before non-master windows start. With per-(partition, idx) sentinel basenames (when those fields are set), the same coordination applies down to RunIdxCommand granularity — though typically Path B is only used at the boot/load level, not inside run-partition.

If no tmux server is running we raise `RuntimeError("interactive_tmux requires a running tmux server — run 'tmux new -s qflex' first, then re-run.")` rather than silently spawning a detached server (libtmux can, but the user wouldn't see it).

`libtmux` is in [requirements.txt](../../../requirements.txt). Lazy-imported inside `_execute_in_tmux` so non-interactive runs don't pay the cost.

## Coexistence

`interaction_script` and `interactive_tmux` can both be set on the same leaf — the bash with the background expect script gets sent to a tmux window. The user sees QEMU + script output and could interrupt the script if they wanted. We don't gate this combination, but it's an unusual combo.

## Dry-run markers

Path A produces standard `[py-touch] .started` / `[bash] …` / `[py-touch] .done` markers. The bash includes the env-var prefix, the `<script> &`, the gdb invocation with both telnet args, and `wait $SCRIPT_PID`. Tests assert on these substrings — see [tests/test_multi_node_ordering.py](../../../tests/test_multi_node_ordering.py) `test_boot_with_interaction_script_dry_run`.

Path B adds a `[tmux] would open new window 'Phase-nodeN' …` line and replaces the closing `[py-touch] .done` with `[py-poll] .done` (Python polls for the marker the bash itself touches, not touches it directly). The dry-run path skips libtmux entirely. Tests for Path B parse the `tmux_window` field on the `DryRunBlock` — see `test_boot_interactive_tmux_dry_run` and the `testing` skill.

## Files

| Path | What it does |
|---|---|
| [commands/config.py](../../../commands/config.py) | `interaction_script`, `interactive_tmux`, `serial_telnet_port` fields on `ExperimentContext` + factory; `compute_runtime_settings` auto-flip + auto-port logic. |
| [commands/qemu.py](../../../commands/qemu.py) | `get_stdio()` emits serial-on-telnet for Path A; `get_qemu_base_args()` emits monitor-on-telnet via `use_telnet_monitor`. Suppresses `-monitor none` when monitor is on telnet. |
| [commands/boot.py](../../../commands/boot.py) | `SUPPORTS_INTERACTIVE = True`; Path A bash assembly in `cmd()`. |
| [commands/load.py](../../../commands/load.py) | Symmetric to boot. |
| [commands/executer.py](../../../commands/executer.py) | `_execute_in_tmux`, `SUPPORTS_INTERACTIVE` flag on `Executor`, dry-run printer extension for `[tmux]` / `[py-poll]`. |
| [requirements.txt](../../../requirements.txt) | `libtmux` dep. |
| [conf/DC/dc-multi.yaml](../../../conf/DC/dc-multi.yaml) | Top-of-file commented example showing per-leaf field usage. |
| [tests/test_multi_node_ordering.py](../../../tests/test_multi_node_ordering.py) | `test_boot_with_interaction_script_dry_run`, `test_load_with_interaction_script_dry_run`, `test_boot_interactive_tmux_dry_run`, `test_interactive_tmux_only_on_boot_and_load`. |

## Common pitfalls

- **Don't mix multiplexed monitor with `expect`.** `mon:stdio` + Ctrl-A C is fine for humans; expect can't reliably parse it. Path A always uses separate telnet ports.
- **Path A auto-flips `use_telnet_monitor`.** When you set `interaction_script` in YAML, `use_telnet_monitor=True` and `serial_telnet_port` are auto-set in `compute_runtime_settings`. The auto-flip prints a one-line note. Don't be surprised that the rendered bash has telnet args even though you didn't set the telnet flags explicitly.
- **Path B requires tmux already running.** No auto-spawn. Run `tmux new -s qflex` first.
- **Path B blocks until the user quits QEMU in every window.** That's by design — downstream phases stay synced. If you want fire-and-forget tmux dispatch, you'd need a separate field; today's behaviour is poll-until-done.
- **`SUPPORTS_INTERACTIVE` is class-level on `Boot`/`Load` only.** `interactive_tmux=True` flowing into `fw` / `run-partition` (the same context is reused across phases via `clone_experiment_context`) is silently ignored — those classes inherit the default `SUPPORTS_INTERACTIVE=False`.
- **Guest agent is future work.** Path A's "drive serial with expect" is the only mechanism today. If/when we add guest-agent support, the natural shape is one more `ExperimentContext` field (`use_guest_agent: bool`) plus a `GUEST_AGENT_SOCKET` env var for the script.
- **The dev image already has expect + telnet.** No package install needed. `libtmux` is the only new Python dep.
