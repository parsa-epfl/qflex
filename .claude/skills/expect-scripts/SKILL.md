---
name: expect-scripts
description: Use when writing, **creating**, editing, or debugging expect scripts under [sample_scripts/](../../../sample_scripts/) (general-purpose), [tests/realrun/](../../../tests/realrun/) (test-specific), or **any `conf/<WORKLOAD>/expects/` directory** (per-workload Path A drivers — e.g. [conf/WSV/expects/](../../../conf/WSV/expects/), [conf/MS/expects/](../../../conf/MS/expects/), [conf/DC/expects/](../../../conf/DC/expects/)). These are the Path A scripts that talk to QEMU over telnet serial + telnet monitor, capture guest output, and shut QEMU down. Covers the **mandatory `chmod +x` after every fresh `Write` of a new `.exp`** (the executor invokes the script directly via bash; missing exec bit silently kills that leaf with `Permission denied` and the failure surfaces on the *other* leaf via the peer-kill — looks like a coordination bug but isn't), connect_telnet polling with named wall-clock budgets, the drain-based ls capture pattern, why prompt-regex anchoring is fragile, the post-loadvm queued-replay race, the master/non-master split for multi-node savevm, the symmetric peer-kill that guarantees both nodes exit, and why the outer `set timeout` does NOT bound Tcl while/sleep loops. **TRIGGER on ANY interaction with a `.exp` file**: writing a new one, editing an existing one, chmod'ing one, or even just adding / changing an `interaction_script:` YAML field that points at one — read this skill BEFORE the first `Write` call, not after. Also fire when the user mentions `connect_telnet`, expect timeouts not firing, "ls didn't return" / partial-capture errors after loadvm, the savevm-done sentinel, or anything else about driving QEMU from expect via Path A. SKIP for the YAML/DI wiring of `interaction_script` beyond the field name itself (qflex-dependency-injection skill), the underlying executor's bash wrapper / mp.Process dispatch (executor skill), or how Path A relates to Path B / tmux (boot-load-interactive skill — that one covers the higher-level architecture; this one covers the script internals).
---

# Writing expect scripts for QEMU under qflex Path A

These are the gotchas you have to know to write expect scripts that don't hang or capture garbage. Most of them surfaced from real bugs and have direct fixes already deployed under [sample_scripts/](../../../sample_scripts/) (general samples) and [tests/realrun/](../../../tests/realrun/) (test-only) — if you're tempted to ignore one, grep the script that already learned it.

## Sleeps default to HOST wall-clock, not the guest

A `send "sleep 3\r"` inside the script types `sleep 3` *into the guest shell*, so that 3 seconds runs in **guest virtual time**. Under PDES `sync=true` with low `latencyns` (the simulation default of 100µs), guest virtual time can advance many orders of magnitude slower than wall-clock — `send "sleep 3\r"` has been observed to take 10+ minutes wall-clock to return, and pytest times out long before it does.

Whenever you reach for a delay, the default must be **host wall-clock**, expressed as Tcl `sleep N` (script-internal proc only — never `send` it) or, if there's any chance the guest is producing output during the wait, `drain_for N` (the canonical proc that actively drains the chardev for N wall-clock seconds — see [tests/realrun/loaded_test_verify_and_swap_workload.exp](../../../tests/realrun/loaded_test_verify_and_swap_workload.exp)). The reason: most "sleep" calls in an expect script exist to give the two emulated nodes' workloads a moment to settle / produce data, not to advance guest time. A `drain_for 3` between "start workload" and "probe pids" is sufficient because the workload's iterations run at qemu's emulation rate, not the wall clock.

**Only send `sleep` to the guest if the user has explicitly asked for guest-side timing** (e.g. "wait 10 guest seconds before X" or "the workload's heartbeat sidecar should sleep 5 between prints"). When in doubt, ask before typing `sleep N` into a `send`. Inside workload bodies installed via heredoc — `echo_server.sh`'s sidecar heartbeat, `sender.sh`'s `usleep` — the sleep is intentionally guest-side because the workload itself is part of the simulation; that's an expected exception, not a counterexample to the default.

Equivalents to use, by intent:

| Intent | Use |
|---|---|
| Pause the script for N wall-clock seconds, no chardev to drain | Tcl `sleep N` (e.g. inside a polling proc between iterations) |
| Pause for N wall-clock seconds AND keep draining qemu's chardev so the TCP buffer doesn't backpressure the guest | `drain_for N` (the canonical proc) |
| Genuinely wait for N seconds of guest time | `send "sleep N\r"; expect -re {[\$#] +}` — and only after explicitly checking with the user that guest-side timing is what they want |

## Don't blind-sleep waiting for a guest prompt — gate on the prompt itself

A pattern that looks reasonable but isn't:

```tcl
send "su\r"
sleep 1
send "\r"        ; # the empty password
sleep 1
send "\r"        ; # spare \r in case the image asks for both user and password
expect { ... -re {# +} }
```

The fixed `sleep 1` is gambling that the guest's `getpass()` has entered its
read loop within 1 s wall-clock. Under PDES `sync=true` (load / fw / init /
run-* phases) the guest's virtual time advances variably vs wall-clock, so
the `\r` can land before `getpass()` is ready — the kernel tty line
discipline folds it into the typed-echo of `su` instead of treating it as the
password answer. `su` then sits waiting for input that never comes; the
script times out at `BUDGET_PROMPT_S` with no `Authentication failure` and
no `#` ever appearing in `qemu_serial.log`.

Manual interaction works because your eyes are the gate — you wait to see
`Password:` before pressing Enter. The script needs the equivalent: gate on
the *actual prompt event*, not a wall-clock proxy.

```tcl
send "su\r"
set timeout $BUDGET_PROMPT_S
expect {
    timeout { puts "ERROR: never reached root prompt after su"; quit_qemu $monitor_port; exit 1 }
    -re {[Aa]uthentication failure|incorrect password} {
        puts "ERROR: su rejected creds"
        quit_qemu $monitor_port; exit 1
    }
    -re {[Ll]ogin: }    { send "\r"; exp_continue }
    -re {[Pp]assword: } { send "\r"; exp_continue }
    -re {# +}
}
```

`exp_continue` re-enters the same `expect` block after a match, so the
script answers each prompt as it arrives — works for images that prompt for
just `Password:` and for ones that prompt for `Login:` then `Password:`.
The explicit auth-failure arm fails fast (within seconds) instead of
masquerading as a `BUDGET_PROMPT_S = 60 s` timeout — important because
"timed out at 60 s" looks like an image / config / network bug; "auth
failed" tells you exactly what's wrong.

Same shape for any other interactive prompt (sshpass, dialog-style
installers, etc.): pattern-match each prompt, `exp_continue`, fail-fast on
the visible error string, fall through on the success terminator. Don't
fight tty line-discipline timing with `sleep` — let `expect` block until the
event you actually need.

## Bringing up the PDES NIC inside the guest

A multi-node guest sees two NICs on the PCI bus, ordered by PCI address:

| Linux iface | QEMU device | PCI addr | Role |
|---|---|---|---|
| `eth0` | virtio-net-pci | 0x10 | PDES inter-node wire (no DHCP server) |
| `eth1` | e1000 | 0x11 | user-mode NAT (DHCP, internet egress) |

OpenRC's networking script tries DHCP on `eth0` at boot and times out —
that's expected: there's no DHCP server on the PDES wire. Your script has
to assign a static IP before any peer-pinging can work. Failure manifests as
`ping: sendto: Network unreachable` in the guest serial — the kernel has no
route to `192.168.100.0/24`.

Pattern (after a post-loadvm settle so OpenRC has finished its initial pass):

```tcl
send "su\r"
# ... prompt-aware expect with empty-cred answers (see "Don't blind-sleep") ...
send "ip link show\r";                                        expect {-re {# +}}
send "ip link set eth0 down\r";                               expect {-re {# +}}
send "ip link set eth0 address 52:54:00:aa:bb:00\r";          expect {-re {# +}}
send "ip link set eth0 up\r";                                 expect {-re {# +}}
send "ip addr add 192.168.100.1/24 dev eth0\r";               expect {-re {# +}}
send "ip addr show eth0\r";                                   expect {-re {# +}}
send "su qflex\r";                                            expect {-re {\$ +}}
```

The MAC mirrors qflex's per-node default — `commands/config.py:373` derives
`mac=52:54:00:aa:bb:<node*10+i>` per (node, neighbour-index). For node 0
neighbour 0 that's `52:54:00:aa:bb:00`; for node 1 neighbour 0 it's
`52:54:00:aa:bb:0a`. The IPs `192.168.100.1` / `.2` are the convention used
by the project's reference tests under [tests/realrun/](../../../tests/realrun/).

`eth0` here is contextual — Alpine probes PCI in addr order, so the
lower-addr device (0x10 = PDES) becomes eth0 in this layout. If you change
`pdes_net_devs` ordering or add more NICs, look up the iface by MAC instead
(see [tests/realrun/loaded_test_create_master.exp](../../../tests/realrun/loaded_test_create_master.exp)
for the BY-MAC pattern using `ip -o link | awk` against the expected MAC).

The bring-up belongs in the **load** phase script, not boot — boot's snapshot
captures pre-OpenRC state, and the OpenRC pass that runs on every loadvm
re-resume re-tries DHCP and may stomp our static IP. Configuring after the
post-loadvm settle, before `savevm loaded`, bakes the configured eth0 into
the `loaded` snapshot, which is what the simulation phases re-resume from.

## Bake working internet into the booted snapshot — eth1 up + udhcpc + DNS

**Default in every new boot expect** that needs guest internet egress (any
workload that later does `apk add`, `curl`, `wget`, …), unless explicitly
told the guest must have no internet. After the post-login `su` (still
root, before `savevm booted`):

```tcl
send "ip link set eth1 up\r";                                    expect -re {# +}
send "udhcpc -i eth1\r";                                          expect -re {# +}
send "echo 'nameserver 10.0.2.3' > /etc/resolv.conf\r";           expect -re {# +}
send "echo 'nameserver 8.8.8.8'  >> /etc/resolv.conf\r";          expect -re {# +}
```

What each line does:

- `ip link set eth1 up` — eth1 (the e1000 NAT NIC at PCI addr 0x11 in
  multi-node; the only NIC in single-node) is sometimes left DOWN by the
  guest's openrc pass. Without it, the next steps have no link.
- `udhcpc -i eth1` — pulls a lease from QEMU's user-mode DHCP (default
  10.0.2.15) and installs the default route via 10.0.2.2. Internet egress
  works after this.
- The two resolv.conf echos: first overwrites whatever udhcpc wrote with a
  clean `nameserver 10.0.2.3` (QEMU's user-mode DNS proxy — fast when it
  works), then **appends** `nameserver 8.8.8.8` as a public-DNS fallback.
  `udhcpc` alone often leaves `/etc/resolv.conf` pointing at only
  `10.0.2.3`, which is flaky and silently drops queries under load
  (observed: `ping: bad address 'google.com'` from a guest with a working
  10.0.2.x lease and default route). The 8.8.8.8 fallback fixes that.

Put it in **boot**, not load: eth1's up state, the lease, the default
route, and `/etc/resolv.conf` are all part of the snapshot. Writing them
once at boot bakes them into every downstream snapshot (`loaded`,
`init_warmed`, sample-unit checkpoints, …). Putting any of it in load
forces every loadvm-resumed run to redo the setup, racing the workload's
networking config.

Skip the rule only when the user says "this guest must have no internet"
or names a specific NIC / DNS server (in which case substitute).

## Outer `set timeout` does NOT bound Tcl while/sleep loops

Expect's `set timeout 900` only fires inside an `expect { … }` block. A bare Tcl `while {true}` with `sleep 2` between iterations is not affected by it. Same for `wait_for_savevm_done`-style file-watching loops. Without a separate wall-clock deadline, a polling loop spins forever the moment its target transitions from "not yet alive" to "dead" — e.g. you're polling QEMU's monitor port via telnet, a peer process kills QEMU, every subsequent attempt gets `Connection refused`, and the loop keeps retrying.

This is the **only** place inside an expect script where a wall-clock budget is justified (per the project's no-cap rule in CLAUDE.md). Use it; the outer `set timeout` and `_exec_in_container(timeout=…)` legitimately don't apply.

## The four named budgets

Every expect script under [sample_scripts/](../../../sample_scripts/) declares the same set at the top — no magic numbers, no fresh budgets per call site:

```tcl
set BUDGET_INITIAL_S        10    ; # first connect to qemu (listener may not be up yet)
set BUDGET_DEFAULT_S         5    ; # anything else — qemu was alive, past 5s = dead
set BUDGET_PROMPT_S         60    ; # post-login shell/password/monitor prompts (kernel up, fast)
set BUDGET_BOOT_LOGIN_S   1800    ; # fresh-boot login prompt — kernel boot under PDES is minutes
set BUDGET_CHECKPOINTING_S 1800   ; # savevm/loadvm/any drain-coordinated op
```

Pick by what the call is waiting for, not by what feels generous:

| Context | Budget | Why |
|---|---|---|
| First `connect_telnet` to serial/monitor at script start | `$BUDGET_INITIAL_S` | qemu's `telnet,server,nowait` listener takes a moment to come up after qemu starts; the script and qemu race. |
| `connect_telnet` later in the script (e.g. inside `quit_qemu`) | `$BUDGET_DEFAULT_S` | If we got this far, qemu was alive at some point. Repeated `Connection refused` past 5s ⇒ qemu is dead, fail fast. |
| **Fresh-boot** wait for `[Ll]ogin: ` (no `loadvm`) | `$BUDGET_BOOT_LOGIN_S` | UEFI + GRUB + kernel boot under multi-node PDES (`quantum size=10000` + WWT sync) runs orders of magnitude slower than wall-clock. 60 s is not enough; observed ~30 s wall-clock to get GRUB countdown from `2s` → `0s`, then much more for the kernel to reach the getty banner. Same class as `BUDGET_CHECKPOINTING_S`. |
| Post-login waits: password, shell prompt, monitor `(qemu) ` not after a checkpoint | `$BUDGET_PROMPT_S` | Once the kernel is up, guest interaction is fast. Keep a tight cap so a wedged shell fails fast. |
| `expect` block waiting for `(qemu) ` after `savevm` / `loadvm` / `delvm` | `$BUDGET_CHECKPOINTING_S` | Distributed savevm drains across all nodes and serialises CPU+memory state — minutes, not seconds. |
| Polling the follower's own `Boot.log` for `savevm log 7` | `$BUDGET_CHECKPOINTING_S` | Per-node savevm finalisation; same class as the master's `(qemu) ` wait. |
| Login prompts on a `loadvm`-resumed run | `$BUDGET_DEFAULT_S` | The snapshot already captured the guest at the prompt — there's no kernel to boot. |

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

## A freshly-`Write`n `.exp` file isn't executable — the failure is silent and lands on the wrong leaf

The executor's bash wrapper invokes the script directly: `<interaction_script> & SCRIPT_PID=$!`. If the file came in as `-rw-r--r--` (e.g. it was created via the assistant's `Write` tool, which doesn't set the executable bit), bash dies on launch with `Permission denied`, no expect ever runs against that QEMU, and the visible failure is the **other** leaf SIGKILL'ing this leaf's QEMU via `_kill_peer_qemus` once it finishes. That looks like a peer-coordination bug — it isn't.

After every `Write` of a new expect script, `chmod +x` it. Confirm with `ls -la` once before the first run. The `Edit` tool preserves modes (so updating an existing executable script is fine); only freshly-created files are at risk.

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

## Multi-node `savevm` is master-only — and the deterministic completion signal is `savevm log 7`

PDES `savevm` issues `DRAIN_START` / `DRAIN_END` across every node — it's a distributed snapshot. The drain side-effect is that **every** node serialises its own CPU+memory into its own per-node qcow2; only the master issues the monitor command, but every node writes a snapshot. So:

- **Wire two different scripts in the YAML.** Master leaf gets a script that issues `savevm`. Follower leaves get a script that does NOT issue `savevm`. If both issue it, the drain is double-driven → corrupt snapshot. The [conf/DC/dc-multi-savevm-create.yaml](../../../conf/DC/dc-multi-savevm-create.yaml) fixture is the reference for per-leaf `interaction_script` wiring.
- **Don't branch a single script on `NODE_NUMBER` if you can avoid it** — keep two files. Each script can refuse to run on the wrong node with an early `if {$node_number > 0} { exit 1 }` (or the master's mirror).

The deterministic "per-node savevm finished" signal is the literal string `savevm log 7` in the bash-redirected `Boot.log` — that's the last `printf` inside QEMU's `save_snapshot()` (`migration/savevm.c`). Use it for sync, **not** sentinel files written from the script (those don't tell you whether the per-node save itself returned) and **not** `expect eof` on the serial spawn (eof reactively waits for `_kill_peer_qemus` to SIGKILL the peer, which is timing-dependent and can land *before* the per-node save has finalised).

### Master script

The master's `(qemu) ` prompt comes back from `savevm` *after* `save_snapshot()` returns, which is *after* `log 7` has been printed. So master gets log 7 implicitly — no explicit poll needed. After the prompt:

```tcl
send "savevm booted\r"
expect {
    timeout { puts "ERROR: savevm didn't return"; exit 1 }
    "(qemu) "
}
sleep 5            ; # host wall-clock, gives followers' per-node saves room to finalise
send "quit\r"
```

The 5 s settle is NOT a polled handshake — it's a buffer between master's drain returning and master's `quit` tearing down PDES, so followers have a beat to finish their own `save_snapshot()`. If you need stronger guarantees, the master can poll for each follower's `node<N>_savevm_done.flag` (the reference test [tests/realrun/boot_login_savevm.exp](../../../tests/realrun/boot_login_savevm.exp) does this) — but for the boot phase the 5 s + the executor's 30 s `_post_exit_grace` is usually enough.

### Follower script

The follower must wait for *its own* `savevm log 7` in *its own* Boot.log — that's the bash-redirected stdout the executor sets up at `$exp_folder/Boot.log`. The canonical proc:

```tcl
proc wait_for_substring {path needle budget_seconds} {
    set deadline [expr {[clock seconds] + $budget_seconds}]
    while {[clock seconds] < $deadline} {
        if {[file exists $path]} {
            set fh [open $path r]; set content [read $fh]; close $fh
            if {[string first $needle $content] >= 0} { return }
        }
        sleep 2
    }
    error "wait_for_substring: $needle never appeared in $path within ${budget_seconds}s"
}

# ... after login + ls + 5 s settle:
wait_for_substring "$exp_folder/Boot.log" "savevm log 7" $BUDGET_CHECKPOINTING_S
exit 0
```

When `wait_for_substring` returns, the follower's per-node save is genuinely done; `exit 0` lets bash exit cleanly. The executor's `_kill_peer_qemus` running 30 s later is then a no-op (the peer's QEMU is already gone).

### Symmetric peer-kill is the safety net, not the primary sync

`_kill_peer_qemus` (in [commands/executer.py](../../../commands/executer.py)) catches the case where one leaf hangs — within ~5 s of one bash exiting, the peer's QEMU is SIGKILL'd. **Don't rely on that for snapshot correctness.** It fires whether or not your per-node save finished. Use `savevm log 7` polling on the follower so each leaf exits cleanly on its own terms; let peer-kill remain the unused safety net.

### Settle pauses between guest interactions: Tcl `sleep N`, NOT `send "sleep N\r"`

Symmetric `sleep 5` (host wall-clock) on **both** master and follower after the post-login shell prompt and after the `ls` prompt gives the guest a beat to settle before the next step (master before opening the monitor for `savevm`; follower before starting the `wait_for_substring` poll). It also keeps the two leaves' wall-clock progress roughly in lockstep so their per-node saves start close together. This is *the* legitimate use of an explicit pause in a sync flow — see "Sleeps default to HOST wall-clock" at the top.

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

## Diagnosing a stuck or failed expect run: three logs tell the truth

When a real-run boot/load is hung or just produced surprising output, read these three files in this order — each answers a different question:

| File | What it tells you |
|---|---|
| `$exp_folder/Boot.err` (or `Load.err`, etc.) | Tcl errors (`send: spawn id … not open`, `wait_for_substring: needle never appeared`), bash error lines (`Permission denied`), and PDES exit notes from QEMU (`Core0 Quantum Count`, `PDES Comm invalid parameters in send`). If this is empty and the run hung, the script is *blocked*, not crashed. |
| `$exp_folder/expect_log.txt` | Bidirectional dialogue between expect and telnet (post-`log_file -noappend`). Compare its tail against the script's flow to find which `expect` block is waiting. Any `Connection closed by foreign host.` line means the spawned telnet's QEMU end died. |
| `$exp_folder/qemu_serial.log` | What QEMU itself sent on the serial chardev (logged by `-chardev socket,…,logfile=…`). Independent of expect — proves whether the guest reached `getty` / printed `qflex login: `. If `[Ll]ogin: ` doesn't appear here, no expect script could possibly have matched it. |

Then cross-check the host:

- `ps -ef | grep qemu-system | grep -v grep` — is QEMU still running? `<defunct>` means it exited but its parent (gdb) hasn't reaped — usually because the parent itself is suspended (e.g. you Ctrl+Z'd the run).
- `ls -la /dev/shm/pdes*` — PDES rings still present? Stale rings from a previous crashed run will collide with a re-run; clean with `./clean_up.sh`.

A specific common pattern: `qemu_serial.log` ends mid-OpenRC, `expect_log.txt` ends at the same point with no error, `Boot.err` is empty → the script is blocked in the login `expect` and just needs more time (or `$BUDGET_BOOT_LOGIN_S` is too tight). Don't read this as "stuck"; read it as "still booting."

## Don't rely on stdin to gdb

If the bash wrapper runs `gdb -ex run --args qemu …`, gdb's stdin is /dev/null (no terminal). When QEMU segfaults on quit-time PDES teardown, gdb prompts `Quit anyway? (y or n)` and your script can't answer. Two options:

- **Don't use gdb in tests** — the qflex CLI exposes `--no-gdb` on `boot`/`load`/`initialize`/`fw`. Tests in [tests/test_chained_pipeline.py / test_dev_*.py / test_boot_login_bootstrap.py](../../../tests/test_dev_container_smoke.py) pass it.
- **If you need gdb**, the [wrap_with_gdb](../../../commands/qemu.py) helper emits `yes | gdb -ex run --args …` so the prompt auto-answers.
