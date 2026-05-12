#!/bin/bash
# Get unique PIDs from listening ports and kill them
netstat -tulpn 2>/dev/null | awk -F'/' '/LISTEN|udp/ {print $1}' | awk '{print $NF}' | grep -E '^[0-9]+$' | sort -u | while read pid; do
    kill -9 "$pid" && echo "Killed $pid" || echo "Failed to kill $pid"
done

# Kill all qemu-system-aar processes
ps -a | grep 'qemu-system-aar' | grep -v grep | awk '{print $1}' | while read pid; do
    kill -9 "$pid" && echo "Killed qemu process $pid" || echo "Failed to kill $pid"
done

ps -a | grep 'vanilla-qemu-sy' | grep -v grep | awk '{print $1}' | while read pid; do
    kill -9 "$pid" && echo "Killed qemu process $pid" || echo "Failed to kill $pid"
done


# TODO explain in docs how old messages either need to be checkpointed or removed
rm -f /dev/shm/*pdes_*
