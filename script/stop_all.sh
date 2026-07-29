#!/bin/bash
# 一键停止所有服务器进程

SERVERS=(GateServer PushServer PushServer2 TaskServer MailerServer UMSServer StatusServer)

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

# MailerServer 是 Node.js 进程，需要用不同方式查找
MAILER_PID=$(pgrep -f "node server.js" -d " " 2>/dev/null | xargs -n1 sh -c 'cat /proc/$1/cmdline 2>/dev/null | tr "\0" " " | grep -q "MailerServer" && echo $1' _ {} 2>/dev/null | head -1)
if [ -z "$MAILER_PID" ]; then
    # 备用方案：直接 grep MailerServer 目录下的 server.js
    MAILER_PID=$(pgrep -f "MailerServer/server.js" 2>/dev/null | head -1)
fi
if [ -n "$MAILER_PID" ]; then
    echo -n "  MailerServer (PID: $MAILER_PID) ... "
    kill $MAILER_PID 2>/dev/null
    echo "SIGTERM sent"
fi

# 如果还有残留，强制杀
for srv in "${SERVERS[@]}"; do
    pids=$(pgrep -x "$srv" 2>/dev/null)
    if [ -n "$pids" ]; then
        echo "  $srv still alive, force killing ..."
        kill -9 $pids 2>/dev/null
    fi
done

# 强制杀 MailerServer (Node.js)
MAILER_PID=$(pgrep -f "MailerServer/server.js" 2>/dev/null | head -1)
if [ -n "$MAILER_PID" ]; then
    echo "  MailerServer still alive, force killing ..."
    kill -9 $MAILER_PID 2>/dev/null
fi

# 停止 Client (Vite dev server)
CLIENT_PID=$(pgrep -f "vite" 2>/dev/null | head -1)
if [ -n "$CLIENT_PID" ]; then
    echo -n "  Client (PID: $CLIENT_PID) ... "
    kill $CLIENT_PID 2>/dev/null
    echo "SIGTERM sent"
    sleep 1
    if pgrep -f "vite" >/dev/null 2>&1; then
        kill -9 $(pgrep -f "vite" | tr '\n' ' ') 2>/dev/null
        echo "  Client force killed"
    fi
fi

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
