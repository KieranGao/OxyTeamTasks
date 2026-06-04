#!/bin/bash
# 一键停止所有服务器进程

SERVERS=(GateServer PushServer PushServer2 TaskServer UMSServer StatusServer)

echo "=== Stopping all servers ==="

for srv in "${SERVERS[@]}"; do
    pids=$(pgrep -x "$srv" 2>/dev/null)
    if [ -n "$pids" ]; then
        echo -n "  $srv (PID: $pids) ... "
        kill $pids 2>/dev/null
        echo "SIGTERM sent"
    else
        echo "  $srv ... not running"
    fi
done

# 等最多 3 秒
sleep 3

# 如果还有残留，强制杀
for srv in "${SERVERS[@]}"; do
    pids=$(pgrep -x "$srv" 2>/dev/null)
    if [ -n "$pids" ]; then
        echo "  $srv still alive, force killing ..."
        kill -9 $pids 2>/dev/null
    fi
done

echo "=== Done ==="
