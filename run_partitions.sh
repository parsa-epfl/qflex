#!/bin/bash
# Runs every partition of the experiment in parallel, from the experiment folder:
#   ./run_partitions.sh <warming ratio> <measurement ratio>
# Each partition writes its output to run/partition_<i>/log and err.
#
# Stopping this script (Ctrl+C, kill, closing the terminal) stops every partition: the trap terminates all the
# processes it started (xargs, every run_flexus.sh and their QEMUs).

cd "$(dirname "$0")" || exit 1

# All descendants of a process, parents first.
descendants() {
    local child
    for child in $(ps -o pid= --ppid "$1"); do
        echo "$child"
        descendants "$child"
    done
}

stop_partitions() {
    # Ignore further signals (e.g. qflex forwarding one more) until every partition is stopped.
    trap '' INT TERM HUP
    echo "run_partitions.sh: stopping every partition" >&2
    # Collect the whole tree before signalling: a killed parent no longer lists its children.
    local pids
    pids=$(descendants $$)
    [ -n "$pids" ] && kill -TERM $pids 2>/dev/null
    exit 130
}
trap stop_partitions INT TERM HUP

folders=(run/partition_*/)
if [ ! -d "${folders[0]}" ]; then
    echo "run_partitions.sh: no partition in run/, run ./qflex partition first" >&2
    exit 1
fi

# Functional warming appends checkpoints to the bxdb log (chunks.log); build the B-tree index (index.bxdb) so the
# timing runs can look pages up directly. It is rebuilt from the log every time.
dbs=(run/*.bxdb)
if [ "${#dbs[@]}" != 1 ] || [ ! -d "${dbs[0]}" ]; then
    echo "run_partitions.sh: expected exactly one bxdb database in run/, found: ${dbs[*]}" >&2
    exit 1
fi
(cd run && ../bin/bxdb convert to-btree "$(basename "${dbs[0]}")") || exit 1

# Run in the background and wait: bash runs the trap only once a foreground command ends, but interrupts `wait`.
printf '%s\n' "${folders[@]}" | xargs -P "${#folders[@]}" -I {} bash -c "cd {} && ./run_flexus.sh $1 $2 > log 2> err" &
wait $!
