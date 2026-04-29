#!/usr/bin/env bash
set -u

for X in {0..15}; do
    echo "[$(date +%T)] Launching partition 0 with X=$X"
    setsid script -q -c "./run-partition-0-p.sh $X" "$PWD/0_${X}_output.log" </dev/null &>/dev/null &
done

echo "[$(date +%T)] All partition-0 jobs launched. Sleeping 60s..."
sleep 60

for X in {0..15}; do
    echo "[$(date +%T)] Launching partition 1 with X=$X"
    setsid script -q -c "./run-partition-1-p.sh $X" "$PWD/1_${X}_output.log" </dev/null &>/dev/null &
done

echo "[$(date +%T)] All 32 jobs launched."