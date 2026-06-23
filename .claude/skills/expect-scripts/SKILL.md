---
name: expect-scripts
description: Use when writing, **creating**, editing, or debugging expect scripts under [sample_scripts/](../../../sample_scripts/) (general-purpose), [tests/realrun/](../../../tests/realrun/) (test-specific), or **any `conf/<WORKLOAD>/expects/` directory** (per-workload Path A drivers — e.g. [conf/WSV/expects/](../../../conf/WSV/expects/), [conf/MS/expects/](../../../conf/MS/expects/)). These are the Path A scripts that talk to QEMU over telnet serial + telnet monitor, capture guest output, and shut QEMU down. Covers the **mandatory `chmod +x` after every fresh `Write` of a new `.exp`** (the executor invokes the script directly via bash; missing exec bit silently kills that leaf with `Permission denied` and the failure surfaces on the *other* leaf via the peer-kill — looks like a coordination bug but isn't), connect_telnet polling with named wall-clock budgets, the drain-based ls capture pattern, why prompt-regex anchoring is fragile, the post-loadvm queued-replay race, the master/non-master split for multi-node savevm, the symmetric peer-kill that guarantees both nodes exit, and why the outer `set timeout` does NOT bound Tcl while/sleep loops. **TRIGGER on ANY interaction with a `.exp` file**: writing a new one, editing an existing one, chmod'ing one, or even just adding / changing an `interaction_script:` YAML field that points at one — read this skill BEFORE the first `Write` call, not after. Also fire when the user mentions `connect_telnet`, expect timeouts not firing, "ls didn't return" / partial-capture errors after loadvm, the savevm-done sentinel, or anything else about driving QEMU from expect via Path A. SKIP for the YAML/DI wiring of `interaction_script` beyond the field name itself (qflex-dependency-injection skill), the underlying executor's bash wrapper / mp.Process dispatch (executor skill), or how Path A relates to Path B / tmux (boot-load-interactive skill — that one covers the higher-level architecture; this one covers the script internals).
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

## The guest is Alpine; its containers are Debian — `apk` outside, `apt` inside

Two different package managers, two different network prerequisites:

- **Inside QEMU you're in Alpine Linux** (busybox + `apk`). To use a tool the
  base image doesn't ship (`iproute2` for `ss`, `socat`, `iptables`, …) you must
  first bring the internet NIC up and lease it — `ip link set eth1 up; udhcpc -i
  eth1` (eth1 = the e1000 NAT at PCI 0x11; see the internet section above) — then
  `apk add <pkg>`. Do this in **boot**, so the tool is baked into the snapshot;
  installing at load time is slow under PDES and may have no egress on that leaf.
- **Don't assume a tool exists across nodes.** The per-node boot scripts differ,
  so a package `apk add`-ed on the follower is NOT on the master. Concrete case:
  the MS boot *follower* installs `iproute2`, the *master* doesn't — so `ss` is
  absent on node 0, and a load-time `ss` probe there silently fails. Prefer a
  busybox built-in / procfs / a tool-free handshake (a `nc_ready` flag + a
  host-clock `sleep`) over probing with a tool one node lacks.
- **Inside a docker container running in the guest you are NOT in Alpine.** The
  cloudsuite / xusine images are Debian/Ubuntu-based, so to add something there
  use `apt-get update && apt-get install -y <pkg>` (and the container needs its
  own egress — it shares the guest's `--net host`, so the eth1 lease above must
  already work).

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

### `[6n` also makes the script feel SLOW — fix with a `\r` nudge, NOT by answering the DSR

`\e[6n` is a Device Status Report: busybox's line editor sends it and then **blocks
reading for the terminal's cursor-position reply** (`\e[…R`), which expect/telnet never
sends. busybox unblocks only when its short read times out or **any input byte arrives**.
That stall is why a run "hangs" at a prompt until you type — the command already ran (its
output streamed); the line-editor is just parked waiting on the reply. Symptom in the logs:
a prompt line ending in `[6n` with no progress until input.

**The fix that works: a `\r` nudge before each wait-for-prompt.** After a command whose
result you await as the next prompt line, `send "\r"` *then* `expect -re {[\$#] +}`. The
`\r` is the input byte that releases busybox's blocked read, so the prompt flushes
immediately. Critically it's a `send`, so it never consumes buffered output. Apply it to
the one-liner drains (`send "cmd\r"; send "\r"; expect -re {# +}`) and the block drains
(`send "\r"` on the line before the `expect {…}`). Skip it on `su`/login auth blocks (they
already answer `Login:`/`Password:` with their own `\r` via `exp_continue`) and on
output-waits that match a specific string rather than the bare prompt.

**Do NOT try to auto-answer the `[6n` with a global `expect_before`.** It looks right
(`expect_before -re "\033\\\[6n" { send "\033\[1;1R"; exp_continue }`) but it cannot work
here, and the failures are instructive (all hit while building the MS load scripts):

1. **`error writing "stdout": bad file number`.** `expect_before` (no `-i`) binds to the
   spawn_id that exists *when it is called*. Placed at the top of the script before any
   `spawn`, it binds to the default user channel, and its `send` writes to stdout —
   corrupting it, so the next plain `puts` dies. (Even fixing this by binding `-i
   $serial_sid` after the spawn doesn't save the approach — see #3.)
2. **`can't read "serial_sid": no such variable`.** The before-body runs in whatever scope
   the triggering `expect` is in. When the DSR fires inside a proc (e.g.
   `wait_for_pane_substring`), a body referencing a top-level var like `$serial_sid` throws,
   because Tcl proc scope doesn't fall back to globals for an explicit `$var`. (A bare
   `send` dodges this — but still see #3.)
3. **The fatal, unfixable one — it eats the prompt.** busybox emits the prompt and the DSR
   glued together: `/home/qflex # \e[6n`. When the `expect_before` pattern matches the
   `\e[6n`, expect consumes the buffer **from the start through the match**, which includes
   the `# ` prompt sitting just before it. So your `-re {# +}` arm never sees the prompt,
   `exp_continue` re-enters on an empty buffer, no more bytes arrive, and the wait times out
   (observed: `ERROR: never reached root prompt after su`). Any expect_before that matches a
   DSR trailing a prompt will swallow that prompt. There is no clean way around it.

So: nudge with `\r` (a non-consuming `send`), don't match-and-answer the DSR. If you ever
truly need to stop busybox emitting `[6n` at all, the only safe lever is guest-side
(`export TERM=dumb` after login) — and it's build-dependent, so verify it actually silences
the query before relying on it.

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
* **ALWAYS pass `-W <timeout>` (e.g. `-W 100`) on a verification ping**, and gate 0-drop on it. The inter-node wire is a *simulated* link: under PDES the one-way `latencyns` plus the icount/Flexus slowdown makes real RTTs hundreds of ms to several seconds (observed 1.5s+, with spikes). `ping`'s default per-packet reply window (~1s) then counts a perfectly-good-but-slow reply as a drop, and your `tx != rx` check aborts a healthy wire. A large `-W` (we use 100s) gives each echo a generous reply window so only a genuinely dead wire fails. The canonical verification ping is therefore `ping <peer> -i 0.5 -W 100 -c 10` — slow interval (no WWT burst), generous reply window (no false drop), small count (fast enough). Size the surrounding `expect` timeout above `-W` (a single slow packet can defer the summary by ~`-W` seconds — e.g. `set timeout 150` for `-W 100`).

This is a real qemu-pdes bug under heavy load — out of scope to fix from an expect script, but the script can avoid triggering it.

## sync=false can drop cross-node packets — raise latency to go fast, don't disable sync

The opposite knob from the burst race above. Turning sync off (`syncs_list:
["false"]`) lets the two nodes' virtual clocks drift apart, so a packet can
arrive stamped *in the past* relative to the receiver's virtual now and get
dropped. Symptom: `ping` shows loss (or 100% loss) and your 0-drop check aborts a
wire that is otherwise fine — even though the same wire works with sync on. So:

- **If a phase must reliably exchange packets** (the ping/nc verification, the
  workload itself), keep `syncs_list: ["true"]`.
- **If you want it to run faster** (the `MSG_TYPE_SYNC` exchange every quantum is
  overhead), don't reach for `sync=false` — instead **raise the per-link
  `latencyns`**. A larger wire latency gives each sender's timestamp enough
  headroom to stay ahead of the receiver's virtual now, so packets land in the
  future (delivered) rather than the past (dropped). Multi-node `boot` uses this
  exact trick: `latencies_ns_list: [1000000]` (1 ms) so it can run `sync=false`
  during pure bring-up; simulation phases keep `sync=true` at 100 µs. If you see
  ping drops under `sync=false`, bump the latency before you suspect the NIC.

## Both nodes must exit, or the test hangs forever

Multi-node tests pair an expect-driven leaf on each node. If one node finishes (cleanly or not) and the other doesn't, the surviving qemu spins forever waiting for PDES sync from the dead peer — pytest, `./qflex load`, and `make test-real-one` then sit through the full outer 30 min `_exec_in_container` timeout. Two layers of defense that already ship together:

- **Executor side (don't touch from a test)** — `Executor._kill_peer_qemus` runs at the end of every multi-node leaf in [commands/executer.py](../../../commands/executer.py). It's symmetric: each leaf, on bash exit, `pkill -9 -f shm-send=/pdes_<neighbor>_to_<my>[part_<P>_][idx_<I>_]` for each of its neighbors. The shm-send pattern is unique to the (peer, partition, idx) tuple so unrelated experiments aren't touched. A `<basename>.killed_by_peer` sentinel is written so the parent `_execute_group` doesn't treat the SIGKILL'd peer's non-zero rc as a real failure.
- **Script side (your responsibility)** — every error branch and every cleanup path must call `quit_qemu` before `exit 1`. Cleanup paths must short-circuit if the monitor is unreachable (see "Cleanup paths must short-circuit" below). Together with the executor's peer-kill, this guarantees that whichever leaf exits first will drag the other down within ~5s.

Don't try to add cross-node coordination at the script level (e.g. "wait for sibling capture sentinel before quitting"). The peer-kill pattern is already in place; layering more sentinels on top tends to introduce the very deadlocks you're trying to avoid.

## Multi-node `savevm` is master-only — and the deterministic completion signal is `savevm log 7`

PDES `savevm` issues `DRAIN_START` / `DRAIN_END` across every node — it's a distributed snapshot. The drain side-effect is that **every** node serialises its own CPU+memory into its own per-node qcow2; only the master issues the monitor command, but every node writes a snapshot. So:

- **Wire two different scripts in the YAML.** Master leaf gets a script that issues `savevm`. Follower leaves get a script that does NOT issue `savevm`. If both issue it, the drain is double-driven → corrupt snapshot. The [tests/realrun/dc-multi-savevm-create.yaml](../../../tests/realrun/dc-multi-savevm-create.yaml) fixture is the reference for per-leaf `interaction_script` wiring.
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

## A "ready" sentinel must mean the resource is ready, not "about to set it up"

A sentinel that one node writes and the other waits on is only correct if it's
written **after** the thing it announces is genuinely usable. The classic bug:
writing the flag right before a blocking command that brings the resource up.
The flag write is an instant host-side `open`; the guest command that actually
binds/listens/starts runs *later* (and under PDES `sync` the guest is orders of
magnitude slower than wall-clock), so the peer races ahead and acts before the
resource exists.

Concrete example (MS multi-node load nc preflight). Master had:

```tcl
open $nc_ready_flag w        ;# instant — host side
send "nc -l -p 443\r"        ;# guest binds the socket SECONDS later, under PDES
expect { -re {hello world} timeout {abort} }
```

Follower waited on `nc_ready`, then `echo 'hello world' | nc -w 2 192.168.100.1 443`.
The flag appeared before the listener was bound; the follower's connect (RTT
~1.3 s on this wire) hit a closed port and the token was lost; master sat 60 s
and aborted, peer-killing the follower. Looks like a coordination bug — it was a
*premature-ready* bug.

Fix pattern (the simple, robust one). Binding a listen socket is a **local
syscall** — it does NOT touch the PDES wire, so it completes in a couple of
wall-clock seconds even under sync. So: master writes the flag *before* starting
the listener, and the peer, on seeing the flag, **sleeps a host-clock interval
(Tcl `sleep`, never a guest `sleep`) before connecting** — long enough to cover
the bind, short because the bind is fast:

```tcl
# master
open $nc_ready_flag w
send "nc -l -p 443\r"          ;# foreground; expect the token next
expect { -re {hello world} timeout {abort} }
send "\003"                     ;# Ctrl-C the listener NOW — see caveat below
expect { -re {# +} timeout { } }

# follower
wait_for_file $nc_ready_flag $BUDGET_WORKLOAD_INIT_S
sleep 10                        ;# HOST wall-clock — bind is a fast local syscall
send "echo 'hello world' | nc -w 10 192.168.100.1 443\r"
```

**Don't wait for a foreground listener to exit on its own** after the token
arrives — `nc -l` only returns once the connection closes, and the peer's
`nc -w 10` timeout is **guest** seconds (minutes of wall-clock under PDES sync),
so both nodes stall (master waiting for its prompt, follower waiting for the next
flag). `\003` kills the listener immediately *and* RSTs the peer's `nc` so it
returns too. Same class as the `sleep` gotcha at the top: any guest-side timeout
(`nc -w`, a blocking read, `sleep`) is guest time — kill the waiter explicitly,
don't time it out.

Don't reach for a tool-based probe (`ss`/`netstat`) to "confirm bound" — the
images differ per node (see the Alpine/apk section below: the boot *follower*
installs iproute2 but the *master* doesn't, so `ss` isn't even on node 0). A
fixed host-clock `sleep` after a truthful "I'm starting it now" flag needs no
tool and no wire. Give the peer's connect a real window too (`-w 10`, not `-w 2`)
when the wire RTT is ~1 s. Same rule applies to socat, a server's listen port, a
tmux pane that must exist, a file the peer reads.

## After wiring any sentinel handshake, trace the whole flow for deadlock / livelock

Writing the `open`/`wait_for_file` pairs is the easy half. Before you run, walk
the two scripts **side by side, top to bottom**, and for every wait answer three
questions:

1. **Who writes the flag I'm waiting on, and have they reached that line by the
   time I block?** A waits for B's flag, B waits for A's flag, and neither writes
   before waiting ⇒ deadlock. Order it so the awaited write always *precedes* the
   wait on the writer's side (master writes `wire_master` **then** waits
   `wire_wait`; follower waits `wire_master` **then** writes `wire_wait`).
2. **Does the flag mean the resource is actually ready** (see the premature-ready
   section above), or just "about to be"? A truthful-looking flag that fires
   early is a livelock/abort waiting to happen — the peer acts, fails silently,
   and someone times out.
3. **On every error/abort path, does this leaf still let the peer make progress
   or die?** If A aborts mid-handshake without `quit_qemu`, B blocks on a flag
   that will never be written for the full outer timeout. Every `exit 1` needs
   `quit_qemu` first so the peer-kill can fire.

Short worked example from this repo: the MS load nc bug above was a #2 failure
(flag written before the listener bound) that *manifested* as a #3 symptom
(master aborts, peer-killed the follower) — and a quick side-by-side read of
"who's bound when the flag is written" is exactly what surfaces it before a
30-minute hung run does. Do this audit every time you add or move a sentinel.

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

## Iterating an `.exp` against a live container until a phase passes

When debugging a Path-A `.exp` end-to-end (multi-node `boot`/`load`), the loop that works is
**edit → run that one phase in a container the user names → poll the per-node logs at ~5 s →
diagnose from the captured guest serial → fix → repeat until it passes.** Concretely:

1. **Ask the user which container to use — every single run, never assume.** They run several
   containers at once (`--pid=host`); a wrong one collides with their other work. The repo is
   bind-mounted into their dev containers, so an `.exp` edit is live immediately — no rebuild
   or restart. (`docker images | grep qflex` shows only the variants present; pick the one the
   user named, e.g. `./dep start-docker --background --container-name <name> --debug` if you
   must bring one up, else `./dep exec --container-name <name> …` into theirs.)
2. **Run only the failing phase, clean first:**
   `./dep exec --container-name <name> --command "./clean_up.sh; ./qflex load -c conf/<WL>/<wl>-multi.yaml"`.
   This auto-backgrounds; the real signal is the on-disk per-node logs, not the tool's stdout.
3. **Poll every ~5 s** with a single foreground bash loop (`for i in $(seq 1 N); do … sleep 5;
   done` — a polling loop with `sleep` between checks is allowed; never `Monitor`/`run_in_background`,
   which write to `/tmp` and fill the boot disk). Each tick: count qemus
   (`docker exec <name> bash -lc "ps -eo comm | grep -c '[q]emu-system'"`), `stat -c%s` each
   node's `Load.log` for growth, and `grep -ac` the phase markers. Break early on success
   markers, on any error marker (`ERROR:`/`Permission denied`/`not a TTY`/`spawn id … not
   open`/`Killed`), on a qemu dropping out, or on a stall (no log growth for ~150 s). Pin any
   workload-handshake step to a hard wall-clock budget (e.g. a client→master token must land
   within ~60 s of being sent) and abort+retry past it instead of waiting out the 24 h budget.
4. **Diagnose from the captured guest serial** — `Load.log`/`Load.err`/`expect_log.txt` per
   node hold the exact bytes the guest echoed. The decisive errors live there (e.g.
   `command not found` = a tool didn't install; `Operation not permitted` on `chmod` = the
   file is root-owned and can't be overwritten by the qflex user; `the input device is not a
   TTY` = a stale `-t` `docker run`). When a `docker exec` produces *no* output at all, suspect
   `docker exec` itself hung — drive commands inside an interactive `docker run -it … bash`
   over the serial instead.
5. **Fix the `.exp`, re-run, repeat** until the phase's success markers all appear and neither
   node's `.err` shows an error. Keep single- and multi-node doing identical *workload* — only
   the per-node plumbing (DNAT, ordering, file-ownership guards) may differ.

### Concrete commands

Locate the per-node folders (pure path computation; filter the factory debug lines):

```bash
./qflex get-experiment-folder -c conf/<WL>/<wl>-multi.yaml | grep '^/'
# -> <group>/, <group>-node-0-0/, <group>-node-1-0/  (logs: <node>/Load.log, Load.err, expect_log.txt)
```

Launch the one phase in the user-named container (auto-backgrounds; watch the on-disk logs):

```bash
./dep exec --container-name <name> --command "./clean_up.sh; ./qflex load -c conf/<WL>/<wl>-multi.yaml"
```

The 5 s monitor loop (one foreground bash call; re-run windows ≤ ~110 iters to stay under the
Bash-tool 600 s cap). `N0`/`N1` = the two `Load.log` paths from `get-experiment-folder`:

```bash
N0=<group>-node-0-0/Load.log; N1=<group>-node-1-0/Load.log
E0=${N0%.log}.err;            E1=${N1%.log}.err
seen_q=0; last=0; stall=0
for i in $(seq 1 110); do
  q=$(docker exec <name> bash -lc "ps -eo comm 2>/dev/null | grep -c '[q]emu-system'"); q=$(echo "$q"|tr -dc 0-9); q=${q:-0}
  s1=$(stat -c%s "$N1" 2>/dev/null); s1=${s1:-0}
  ok=$(grep -ac "<phase success marker, e.g. savevm log 7>" "$N1" 2>/dev/null)   # grep -c already prints 0 — do NOT add `|| echo 0` (it double-prints and breaks `[ ]`)
  bad=$(cat "$N0" "$N1" "$E0" "$E1" 2>/dev/null | grep -acE "ERROR:|Permission denied|Operation not permitted|not a TTY|spawn id exp5 not open|Killed")
  [ "$q" -gt 0 ] && seen_q=1
  if [ "$s1" -eq "$last" ]; then stall=$((stall+1)); else stall=0; fi; last=$s1
  echo "[$i $(date +%H:%M:%S)] qemu=$q n1=$s1 ok=$ok bad=$bad stall=${stall}x5s"
  [ "$ok"  -ge 1 ] && { echo VERDICT=SUCCESS; break; }
  [ "$bad" -ge 1 ] && { echo VERDICT=FAIL_ERROR; break; }
  [ "$seen_q" -eq 1 ] && [ "$q" -eq 0 ] && { echo VERDICT=ENDED; break; }
  [ "$stall" -ge 40 ] && { echo VERDICT=STALL; break; }   # ~200 s no growth (raise threshold while a node idle-waits for its peer)
  sleep 5
done; tail -6 "$N1"
```

Stop a run **scoped to this experiment only** (never blanket-kill on `--pid=host` — the dev
containers share the host PID namespace, so a bare `pkill qemu-system-aarch64` / `./clean_up.sh`
kills the user's *other* concurrent experiments too, in every container). Also note `pkill -f
'<shm-prefix>'` self-matches its own shell and dies with exit 137 before cleaning shm. So kill
**by PID**, gated on the experiment group, via the `[q]emu` bracket trick (which excludes this
shell), then remove the shm:

```bash
docker exec <name> bash -lc '
  ps -eo pid,args | grep "[q]emu-system-aarch64" | grep "<group>" | awk "{print \$1}" | xargs -r kill -9
  rm -f /dev/shm/*<group>*'
```

Why this is correctly scoped on two axes:
- **Both qemu binaries, no others.** `[q]emu-system-aarch64` matches the substring
  `qemu-system-aarch64`, which is present in **both** the parallel/FW binary `./qemu-system-aarch64`
  (boot/load/init/fw) **and** the timing binary `./vanilla-qemu-system-aarch64` (run-* phases) —
  so the same pattern stops the run regardless of phase, while still hitting nothing but qemu.
- **Only the target experiment.** The second `grep "<group>"` (the group name, e.g.
  `data-serving-multi-node-0`, which appears verbatim in each qemu's `shm-send=/…_pdes_…` /
  `shm-recv=` args) limits the kill to this run's processes inside `<name>` — every other
  experiment's qemus, in this or any other container, are left untouched.

`<group>` = the group experiment name from `./qflex get-experiment-folder` (the part before
`-node-<N>-<idx>`). Use the same `*<group>*` glob for the shm files, since `setup_nic_args`
prefixes the group name onto every `/dev/shm/pdes_*` ring it creates.

## Warmup vs load: gate on a warmup-DONE marker, and give each phase its own output file

The client workload runs a **warmup/prep** phase (populates state) then the measured **load/run**
phase. Two rules — both learned from a DS regression (commit `ed1c333` "V1 of expect rework")
that deleted the warmup gate and merged the two phases into one log, so `savevm` fired ~10 s into
warmup and the run phase never happened:

1. **Wait for an explicit warmup-DONE marker before watching for the load marker — never gate the
   run on a line the warmup phase also emits.** YCSB prints `current ops/sec; est completion in`
   during *both* its insert(warmup) and run(load) phases, so that line alone is ambiguous. Gate
   instead on the phase-completion banner the warmup prints, then the load line:
   - **DS:** warmup (`ycsb load`) ends with a `Warm up is done.` banner → wait `-re {Warm up is
     done\.}`; the run phase (`ycsb run` workloadc, READs) then emits `-re {current ops/sec; est
     completion in}`.
   - **MS:** warm ends with `-re {Compressed history:}`; the load driver then prints
     `-re {All tasks are created\.}` (×4).
   - **WSV/WS (Faban):** ramp-up *is* the warmup within one process — gate on a real success first
     (`Successfully browsed Elgg`) then `Ramp up completed`; don't gate on the timer line alone.

2. **Never write warmup and load output to the same file.** Give each phase a **separately-named**
   file so a `tail`/`grep` for the load marker structurally cannot match a warmup line.
   **Keep it ONE container** (do NOT split warmup and load into separate `docker run`s — they
   often share local state, e.g. MS's warm history feeds hammar). Mechanism: the entry script
   runs both phases in the one container but redirects each to its own file under a mounted
   guest-owned dir, and the guest tails each file separately:
   ```sh
   # <wl>_client_entry.sh (one container, two files):
   #!/bin/sh
   ./warmup ... > /out/<wl>_warmup.out 2>&1
   ./load   ... > /out/<wl>_load.out   2>&1
   ```
   ```tcl
   # run_<wl>.sh docker run adds:  -v $(pwd)/<wl>out:/out
   # guest, before launch — clear stale logs (so an old 'Warm up is done.' can't false-match)
   # and pre-create so `tail -f` doesn't race file creation:
   send "mkdir -p <wl>out; rm -f <wl>out/*.out; touch <wl>out/<wl>_warmup.out <wl>out/<wl>_load.out\r"
   # then: tail -f <wl>out/<wl>_warmup.out -> warmup-done marker -> ^C
   #       tail -f <wl>out/<wl>_load.out   -> load marker        -> ^C -> savevm
   ```
   Use a guest-**owned** dir (e.g. `$(pwd)/<wl>out` under the user's home), not sticky `/tmp`, so
   the guest can `rm` the prior run's (root-owned, written by the container) logs.
