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

# 停止 Kafka
KAFKA_HOME="/usr/local/kafka"
KAFKA_PID=$(pgrep -f "kafka.Kafka" 2>/dev/null | head -1)
if [ -n "$KAFKA_PID" ]; then
    echo -n "  Kafka (PID: $KAFKA_PID) ... "
    "$KAFKA_HOME/bin/kafka-server-stop.sh" 2>/dev/null
    sleep 2
    if pgrep -f "kafka.Kafka" >/dev/null 2>&1; then
        kill -9 $(pgrep -f "kafka.Kafka" | tr '\n' ' ') 2>/dev/null
        echo "force killed"
    else
        echo "stopped"
    fi
else
    echo "  Kafka ... not running"
fi

echo "=== Done ==="
