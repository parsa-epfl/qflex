---
name: expect-scripts
description: Use when writing or debugging expect scripts under [sample_scripts/](../../../sample_scripts/) (general-purpose) or [tests/realrun/](../../../tests/realrun/) (test-specific) — the Path A drivers that talk to QEMU over telnet serial + telnet monitor, capture guest output, and shut QEMU down. Covers connect_telnet polling with named wall-clock budgets, the drain-based ls capture pattern, why prompt-regex anchoring is fragile, the post-loadvm queued-replay race, the master/non-master split for multi-node savevm, the symmetric peer-kill that guarantees both nodes exit, and why the outer `set timeout` does NOT bound Tcl while/sleep loops. TRIGGER when the user mentions an `.exp` script, the `interaction_script` field, connect_telnet hangs, expect timeouts not firing, "ls didn't return" / partial-capture errors after loadvm, the savevm-done sentinel, or anything else about driving QEMU from expect via Path A. SKIP for the YAML/DI wiring of `interaction_script` (qflex-dependency-injection skill), the underlying executor's bash wrapper / mp.Process dispatch (executor skill), or how Path A relates to Path B / tmux (boot-load-interactive skill — that one covers the higher-level architecture; this one covers the script internals).
---

# Writing expect scripts for QEMU under qflex Path A

These are the gotchas you have to know to write expect scripts that don't hang or capture garbage. Most of them surfaced from real bugs and have direct fixes already deployed under [sample_scripts/](../../../sample_scripts/) (general samples) and [tests/realrun/](../../../tests/realrun/) (test-only) — if you're tempted to ignore one, grep the script that already learned it.

## Outer `set timeout` does NOT bound Tcl while/sleep loops

Expect's `set timeout 900` only fires inside an `expect { … }` block. A bare Tcl `while {true}` with `sleep 2` between iterations is not affected by it. Same for `wait_for_savevm_done`-style file-watching loops. Without a separate wall-clock deadline, a polling loop spins forever the moment its target transitions from "not yet alive" to "dead" — e.g. you're polling QEMU's monitor port via telnet, a peer process kills QEMU, every subsequent attempt gets `Connection refused`, and the loop keeps retrying.

This is the **only** place inside an expect script where a wall-clock budget is justified (per the project's no-cap rule in CLAUDE.md). Use it; the outer `set timeout` and `_exec_in_container(timeout=…)` legitimately don't apply.

## The three named budgets

Every expect script under [sample_scripts/](../../../sample_scripts/) declares the same trio at the top — no magic numbers, no fresh budgets per call site:

```tcl
set BUDGET_INITIAL_S        10    ; # first connect to qemu (listener may not be up yet)
set BUDGET_DEFAULT_S         5    ; # anything else — qemu was alive, past 5s = dead
set BUDGET_CHECKPOINTING_S 1800   ; # savevm/loadvm/any drain-coordinated op
```

Pick by what the call is waiting for, not by what feels generous:

| Context | Budget | Why |
|---|---|---|
| First `connect_telnet` to serial/monitor at script start | `$BUDGET_INITIAL_S` | qemu's `telnet,server,nowait` listener takes a moment to come up after qemu starts; the script and qemu race. |
| `connect_telnet` later in the script (e.g. inside `quit_qemu`) | `$BUDGET_DEFAULT_S` | If we got this far, qemu was alive at some point. Repeated `Connection refused` past 5s ⇒ qemu is dead, fail fast. |
| `expect` block waiting for `(qemu) ` after `savevm` / `loadvm` / `delvm` | `$BUDGET_CHECKPOINTING_S` | Distributed savevm drains across all nodes and serialises CPU+memory state — minutes, not seconds. |
| Polling for the multi-node `savevm_done.flag` sentinel | `$BUDGET_CHECKPOINTING_S` | Same — non-master is waiting on master's checkpointing. |
| Login prompts, `expect`-ing shell prompts, monitor `(qemu) ` prompts not following a checkpoint op | `$BUDGET_DEFAULT_S` | Standard guest interaction is fast. |

The canonical `connect_telnet` proc taking the budget as an argument:

```tcl
proc connect_telnet {host port {budget_seconds 5}} {
    global timeout
    set saved_timeout $timeout
    set timeout 5
    set deadline [expr {[clock seconds] + $budget_seconds}]
    while {[clock seconds] < $deadline} {
        spawn telnet $host $port
        set this_sid $spawn_id
        set ok 0
        expect {
            -re "Escape character is" { set ok 1 }
            -re "Connection refused" { }
            timeout { }
            eof { }
        }
        if {$ok} { set timeout $saved_timeout; return $this_sid }
        catch {close -i $this_sid}
        catch {wait -i $this_sid}
        sleep 2
    }
    set timeout $saved_timeout
    error "connect_telnet: gave up after ${budget_seconds}s polling $host:$port"
}
```

Default is `$BUDGET_DEFAULT_S` (`5`) so call sites that omit the argument fail fast on a dead port. Initial connections at the top of the script must explicitly pass `$BUDGET_INITIAL_S`. `quit_qemu` uses the default. File-watching procs (e.g. `wait_for_savevm_done`) take the budget as an explicit argument — pass `$BUDGET_CHECKPOINTING_S` from the caller.

## `spawn telnet` does not fail when telnet fails

`spawn telnet 127.0.0.1 9999` returns success even if the port is dead. `telnet` prints `Connection refused` to its stdout and exits. You have to *read what telnet wrote* via an `expect` block matching `Escape character is` (success) vs `Connection refused` (fail). Don't gate on `[catch {spawn …}]`.

## Busybox's prompt sends ESC[6n

Alpine's busybox shell renders the prompt with a trailing `\e[6n` (cursor-position report request). The prompt regex therefore must NOT anchor to end-of-buffer — `\$ $` never matches. Use `[\$#] +` (no end anchor) instead. The `[6n` bytes still leak into `expect_out(buffer)` and end up in your captured `ls` output; that's expected, not a bug.

## Who the user is at the prompt

The guest's hostname is `qflex` (set during base-image creation), not `alpine`. The shell prompt is `qflex:~$ ` for the unprivileged `qflex` user and `qflex:~#` for `root`. **Don't anchor on `alpine:`** — that string never appears at the prompt. Match `[\$#] +` for "any prompt regardless of user", or `qflex:[^\r\n]*[\$#] +` if you specifically want to confirm you're at this guest's shell (and not in some other tool that also prints a `$`).

## loadvm replays the queued serial buffer

When QEMU resumes from `-loadvm`, the serial output that was in flight at savevm time gets replayed once you reconnect telnet. The shell's pre-snapshot prompt characters re-emerge in the buffer **alongside** any prompt your post-restore `\r` triggers. Several variations of "drain the buffer first then anchor on a prompt regex" all failed: bytes can stream in slowly enough that drain returns prematurely, then the next expect matches a stale prompt instead of waiting for our command's output. Don't go down that road.

## Drain-based capture is the robust pattern (the one that ships)

Don't try to anchor `expect` on a fresh prompt to delimit a command's output — that race is unwinnable when the kernel echoes the typed command back as `qflex:~$ <cmd>` (the `[\$#] +` regex will happily match the `$ ` *inside* that echo before `<cmd>` runs). The shipping pattern in [tests/realrun/loaded_test_verify_and_swap_workload.exp](../../../tests/realrun/loaded_test_verify_and_swap_workload.exp) skips prompt anchoring entirely:

```tcl
# Drain anything queued from loadvm replay first.
set saved_timeout $timeout
set timeout 5
expect { -re {.+} { exp_continue } timeout { } }

send "ls\r"
sleep 1                ; # let the guest actually run ls before we look
set ls_output ""
set timeout 2
expect {
    -re {.+} {
        append ls_output $expect_out(buffer)
        exp_continue
    }
    timeout { }
}
set timeout $saved_timeout

if {[string length $ls_output] == 0} {
    puts "ERROR: no bytes received after ls"
    quit_qemu $monitor_port
    exit 1
}
```

Why it works:
- The pre-send drain swallows whatever loadvm replayed; it doesn't matter how many duplicate prompts are queued.
- `sleep 1` is not a fake timeout — it's giving the guest a beat to actually execute `ls` before we read.
- The post-send drain returns `2s` after the last byte arrives, so we capture all of `ls`'s output regardless of how it streams.
- The captured `$ls_output` is just bytes. The test asserts `'test1.txt' in ls_output` — substring search, no parsing.
- No prompt regex means no race.

Don't do the pre-send drain on a *boot* script (fresh boot has no replay, drain just wastes 5s). Do drain after sending the command — that's universal.

## Marker-based capture (the alternative when drain is too coarse)

When you need *one* capture per command across MULTIPLE commands in sequence (and don't want each capture to pay a 2s drain timeout), the more surgical pattern is to have the guest emit a unique end-of-capture marker — see [tests/realrun/loaded_test_verify_and_swap_workload.exp](../../../tests/realrun/loaded_test_verify_and_swap_workload.exp)'s `capture_to`:

```tcl
proc capture_to {out_name shell_cmd} {
    global exp_folder spawn_id
    send "$shell_cmd; printf '\\nQFLEXEOC%s\\n' \"\$\$\"\r"
    expect {
        timeout { puts "ERROR: '$shell_cmd' didn't return end-of-capture marker" }
        -re {\nQFLEXEOC[0-9]+\r?\n}
    }
    set buf $expect_out(buffer)
    expect { timeout { } -re {[\$#] +} }   ; # drain post-marker prompt
    # write $buf to $exp_folder/$out_name
}
```

Why this works (and why the obvious alternatives don't):

* **`expect -re {[\$#] +}`** alone (one-stage prompt match) races stale prompt bytes already in the buffer — e.g. the post-`\r`-nudge prompt that the previous expect call's match left positioned just before. Single-stage gives a 12-byte capture of `qflex:~$ ` instead of the real output.
* **Two-stage `"$shell_cmd\r"` then prompt** races busybox's *prompt redraw* on slower setups: when a command is dispatched while the shell is settling (post-`pkill`, post-loadvm), the typed bytes echo back BEFORE the shell prints its prompt. Then the shell prints `qflex:~$ <command>` (a redraw with the typed line). The stage-2 prompt regex then matches the redraw's `$ ` BEFORE the command's actual output streams in — `capture_to` returns with the wrong buffer (the next call captures what should have been here).
* **The PID-bearing marker** can't appear in the typed echo (the typed bytes have the literal `%s`, not a digit), so the regex `\nQFLEXEOC[0-9]+\r?\n` only matches the printf's actual output. No race.

Pytest tolerates the typed-echo prefix in the captured buffer via regex / `key=value` parsing — see how `tests/test_chained_pipeline.py::test_03b` reads `ping_summary_after_loadvm.txt` and `workload_state.txt`.

Pick one of:
* **Drain-based** when you have ONE capture (e.g. boot scripts that run a single `ls`) and a 2s post-send wait is acceptable.
* **Marker-based** when you have a sequence of captures back-to-back and need each one's buffer to be unambiguously its own command's output.

## Sync-on PDES wires don't tolerate fast bursts

`sync=true` + low `latencyns` (the load/init/fw default of 100µs) is fragile under sustained network bursts: `wwt_recivied_callback` will fire its `Received message with timestamp in the past` assertion and abort qemu. The threshold is around `ping -i 0.001` (1ms intervals); `-i 0.1` (100ms) is safe. When designing a workload to bake into a snapshot:

* **Throttle every loop** that talks to the peer over the PDES NIC. The `urandom→nc→cmp` sender in `loaded_test_verify_and_swap_workload.exp` has `sleep 1` between iterations — without it, the WWT race fires within seconds of workload start.
* **Don't leave a `ping -i 0.001 -c 1000000` running across savevm/loadvm.** When the snapshot is later loaded, the ping process resumes and immediately starts blasting the wire — the WWT race fires before any test logic can step in. The load script `pkill`s any leftover ping as its very first action post-loadvm; mirror that pattern in any script that loads a snapshot known to contain a fast pinger.
* **The peer-ping verification at script start should also be slow.** Ten packets at 100ms is enough to confirm "the wire works"; 100 at 1ms is a race trigger.

This is a real qemu-pdes bug under heavy load — out of scope to fix from an expect script, but the script can avoid triggering it.

## Both nodes must exit, or the test hangs forever

Multi-node tests pair an expect-driven leaf on each node. If one node finishes (cleanly or not) and the other doesn't, the surviving qemu spins forever waiting for PDES sync from the dead peer — pytest, `./qflex load`, and `make test-real-one` then sit through the full outer 30 min `_exec_in_container` timeout. Two layers of defense that already ship together:

- **Executor side (don't touch from a test)** — `Executor._kill_peer_qemus` runs at the end of every multi-node leaf in [commands/executer.py](../../../commands/executer.py). It's symmetric: each leaf, on bash exit, `pkill -9 -f shm-send=/pdes_<neighbor>_to_<my>[part_<P>_][idx_<I>_]` for each of its neighbors. The shm-send pattern is unique to the (peer, partition, idx) tuple so unrelated experiments aren't touched. A `<basename>.killed_by_peer` sentinel is written so the parent `_execute_group` doesn't treat the SIGKILL'd peer's non-zero rc as a real failure.
- **Script side (your responsibility)** — every error branch and every cleanup path must call `quit_qemu` before `exit 1`. Cleanup paths must short-circuit if the monitor is unreachable (see "Cleanup paths must short-circuit" below). Together with the executor's peer-kill, this guarantees that whichever leaf exits first will drag the other down within ~5s.

Don't try to add cross-node coordination at the script level (e.g. "wait for sibling capture sentinel before quitting"). The peer-kill pattern is already in place; layering more sentinels on top tends to introduce the very deadlocks you're trying to avoid.

## Multi-node `savevm` is master-only

PDES `savevm` issues `DRAIN_START` / `DRAIN_END` across every node — it's a distributed snapshot. The script that runs on **node 0** (master) issues the savevm; non-master scripts must STAY ALIVE during the drain (their QEMU processes have to participate) until the master signals completion. The standard wiring under [sample_scripts/](../../../sample_scripts/) is:

- master → [boot_create_and_savevm_master.exp](../../../sample_scripts/boot_create_and_savevm_master.exp): does work, issues `savevm`, **writes a sentinel file** (`<group_folder>/savevm_done.flag`), then quits.
- non-master → [boot_create_and_wait.exp](../../../sample_scripts/boot_create_and_wait.exp): does work, **polls the sentinel** (`while {![file exists $flag]} {sleep 2}`), then quits.

If you wire the same script to both leaves, you double-drive the drain and produce a corrupt snapshot. If you forget to keep non-master alive past savevm, the drain tears down mid-coordination. The [conf/DC/dc-multi-savevm-create.yaml](../../../conf/DC/dc-multi-savevm-create.yaml) fixture is the reference for how to wire two different `interaction_script` values per leaf.

## Capture order matters for what you're testing

`ls > capture > savevm` versus `savevm > capture` are different tests. The first proves "the file exists in the running guest", which is uninteresting — `touch` already returned. The second proves "the file is in the snapshotted state", which is the only thing `loadvm` later verifies against. If your test is asserting that loadvm restores correctly, the boot script must capture **after** savevm completes (and after the savevm-done sentinel for non-master). See the master/wait script docstrings for the rationale.

## QEMU's host channels — don't conflate

| Channel | What it does | QEMU flag | Don't conflate with |
|---|---|---|---|
| Serial console | guest's `/dev/ttyAMA0`; you talk to login prompts and shells here | `-serial telnet:127.0.0.1:<serial_port>,server,nowait` | the monitor — issuing `savevm` over serial sends those bytes to the guest shell |
| HMP monitor | QEMU's own command surface (`savevm`, `quit`, `info snapshots`, …) | `-monitor telnet:127.0.0.1:<monitor_port>,server,nowait` | the guest agent — `savevm` is HMP, not a guest command |
| QEMU guest agent (qemu-ga) | RPC into the guest via virtio-serial | `-chardev socket,…` + `-device virtserialport,…` | the HMP monitor — qmp/qga share QMP framing but live on different transports |

The qflex executor wires `compute_runtime_settings` to set `telnet_port = 55558 + node_number` (monitor) and `serial_telnet_port = 55600 + node_number` (serial), and exports them to your script as `TELNET_MONITOR_PORT` and `TELNET_SERIAL_PORT`. Use the right one for the right job: serial for typing into the guest, monitor for telling QEMU what to do.

## Always quit QEMU on every exit path

The bash wrapper around your script does `wait $SCRIPT_PID` after gdb/qemu — if QEMU is still running when expect exits, that wait blocks forever. Every error branch in your script should call `quit_qemu $monitor_port` before `exit 1`, even if you doubt QEMU is still alive (it's idempotent — quit_qemu's connect_telnet either succeeds and sends `quit`, or hits its budget and returns).

## Cleanup paths must short-circuit once qemu is gone

Once your script has finished its real work (captured the file, written its sentinel, etc.), every remaining call is best-effort. If `quit_qemu` can't reach the monitor port within the default budget, the image has already exited — return immediately, don't keep retrying. Wrap the cleanup `connect_telnet` in a `catch` and bail on failure:

```tcl
proc quit_qemu {monitor_port} {
    global BUDGET_DEFAULT_S
    if {[catch {connect_telnet 127.0.0.1 $monitor_port $BUDGET_DEFAULT_S} sid]} {
        return
    }
    set timeout $BUDGET_DEFAULT_S
    expect {
        -i $sid
        timeout { puts "quit_qemu: monitor didn't prompt" }
        "(qemu) " {
            send -i $sid "quit\r"
            catch {expect -i $sid eof}
        }
    }
}
```

The bash wrapper that's `wait`-ing on this script should never sit there for extra minutes after the captures are written. If you find yourself adding a `sleep N` before `quit_qemu` to "give siblings time to finish", question whether you're working around a missing short-circuit somewhere else — usually the right fix is to make the cleanup path silently return when the resource is gone, not to globally pad with seconds.

## Don't rely on stdin to gdb

If the bash wrapper runs `gdb -ex run --args qemu …`, gdb's stdin is /dev/null (no terminal). When QEMU segfaults on quit-time PDES teardown, gdb prompts `Quit anyway? (y or n)` and your script can't answer. Two options:

- **Don't use gdb in tests** — the qflex CLI exposes `--no-gdb` on `boot`/`load`/`initialize`/`fw`. Tests in [tests/test_chained_pipeline.py / test_dev_*.py / test_boot_login_bootstrap.py](../../../tests/test_dev_container_smoke.py) pass it.
- **If you need gdb**, the [wrap_with_gdb](../../../commands/qemu.py) helper emits `yes | gdb -ex run --args …` so the prompt auto-answers.
