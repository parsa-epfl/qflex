#!/usr/bin/env bash
# Per-qemu guest RSS vs -m cap (+ how much the host swapped each out), then host
# memory and live swap activity. RSS is the real physical footprint; -m is only a
# ceiling. RSS ~= cap => guest is using all its RAM; nonzero `so` => host swapping.
set -uo pipefail

printf '%-10s %-18s %-7s %-18s %s\n' PID "RSS / cap" "%cap" "host-swapped-out" image
printf '%.0s-' {1..78}; echo

ps -eo pid,rss,args | awk '/[q]emu-system/ && $2 > 1048576 {print $1, $2}' | while read -r pid rss; do
    cmd=$(tr '\0' ' ' < "/proc/$pid/cmdline" 2>/dev/null) || continue
    cap=$(grep -oE '\-m [0-9]+' <<<"$cmd" | awk '{print $2}')
    img=$(grep -oE 'file=[^, ]+qcow2' <<<"$cmd" | head -1 | sed 's#.*/##')
    sw=$(awk '/VmSwap/{print $2}' "/proc/$pid/status" 2>/dev/null)
    awk -v p="$pid" -v r="$rss" -v c="${cap:-0}" -v s="${sw:-0}" -v i="$img" 'BEGIN{
        printf "%-10s %5.1f / %-4g GB  %4.0f%%  %8.2f GB        %s\n",
               p, r/1048576, c/1024, (c>0?100*r/1024/c:0), s/1048576, i
    }'
done

echo
echo "Host memory + swap:"
free -h
echo
echo "Swap activity (watch si/so KB/s; nonzero so = actively swapping):"
vmstat 1 3
