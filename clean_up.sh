#!/bin/bash

# Get unique PIDs from listening ports and kill them
netstat -tulpn 2>/dev/null | awk -F'/' '/LISTEN|udp/ {print $1}' | awk '{print $NF}' | grep -E '^[0-9]+$' | sort -u | while read pid; do
    kill -9 "$pid" && echo "Killed $pid" || echo "Failed to kill $pid"
done

rm /dev/shm/pdes*

