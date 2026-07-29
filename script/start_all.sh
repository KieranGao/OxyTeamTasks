#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGDIR="$BASE/server_logs"
mkdir -p "$LOGDIR"

echo "=== Starting all servers ==="

# 0. Kafka (日志中间件，所有服务依赖)
KAFKA_HOME="/usr/local/kafka"
if nc -z -w2 127.0.0.1 9092 2>/dev/null; then
    echo "  [0/7] Kafka (port 9092) ... already running"
else
    echo -n "  [0/7] Kafka (port 9092) ..."
    mkdir -p /home/oxythecrack/kafka-data
    "$KAFKA_HOME/bin/kafka-server-start.sh" -daemon "$KAFKA_HOME/config/kraft/server.properties" 2>/dev/null
    # 等待 Kafka 就绪（最多 20 秒）
    for i in $(seq 1 20); do
        if nc -z -w2 127.0.0.1 9092 2>/dev/null; then
            echo " started"
            break
        fi
        sleep 1
    done
    if ! nc -z -w2 127.0.0.1 9092 2>/dev/null; then
        echo " FAILED (timeout)"
    fi
    # 确保内部 topic 存在
    "$KAFKA_HOME/bin/kafka-topics.sh" --bootstrap-server 127.0.0.1:9092 --create --topic __consumer_offsets --partitions 50 --replication-factor 1 --if-not-exists 2>/dev/null
    "$KAFKA_HOME/bin/kafka-topics.sh" --bootstrap-server 127.0.0.1:9092 --create --topic logs --partitions 1 --replication-factor 1 --if-not-exists 2>/dev/null
fi

# 1. StatusServer (依赖项，先启动)
echo "  [1/7] StatusServer (port 50052)..."
cd "$BASE/StatusServer/build"
./StatusServer > "$LOGDIR/StatusServer.log" 2>&1 &
sleep 1

# 2. MailerServer (Node.js, 依赖 StatusServer 心跳 + Redis)
echo "  [2/7] MailerServer (grpc:50051)..."
cd "$BASE/MailerServer"
node server.js > "$LOGDIR/MailerServer.log" 2>&1 &
sleep 1

# 3. UMSServer
echo "  [3/7] UMSServer (port 50053)..."
cd "$BASE/UMSServer/build"
./UMSServer > "$LOGDIR/UMSServer.log" 2>&1 &
sleep 1

# 4. TaskServer
echo "  [4/7] TaskServer (port 50054)..."
cd "$BASE/TaskServer/build"
./TaskServer > "$LOGDIR/TaskServer.log" 2>&1 &
sleep 1

# 5. PushServer1
echo "  [5/7] PushServer1 (ws:8890 grpc:50056)..."
cd "$BASE/PushServer/build"
./PushServer > "$LOGDIR/PushServer.log" 2>&1 &
sleep 1

# 6. PushServer2
echo "  [6/7] PushServer2 (ws:8891 grpc:50057)..."
cd "$BASE/PushServer2/build"
./PushServer2 > "$LOGDIR/PushServer2.log" 2>&1 &
sleep 1

# 7. GateServer
echo "  [7/7] GateServer (port 8080)..."
cd "$BASE/GateServer/build"
./GateServer > "$LOGDIR/GateServer.log" 2>&1 &
sleep 2

# 8. Client (Vite dev server)
echo "  [8/8] Client (port 3000)..."
cd "$BASE/Client"
nohup npm run dev > "$LOGDIR/Client.log" 2>&1 &
sleep 2

# 健康检查
echo ""
echo "=== Health Check ==="
sleep 1

check_http() {
    local desc="$1"
    local url="$2"
    local resp=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout 3 "$url" 2>/dev/null)
    if [ "$resp" = "200" ] || [ "$resp" = "404" ] || [ "$resp" = "405" ]; then
        echo "  [OK] $desc ($resp)"
    else
        echo "  [FAIL] $desc (http_code=$resp)"
    fi
}

check_grpc() {
    local desc="$1"
    local addr="$2"
    if nc -z -w2 ${addr%:*} ${addr#*:} 2>/dev/null; then
        echo "  [OK] $desc (port open)"
    else
        echo "  [WARN] $desc (port closed)"
    fi
}

check_grpc "Kafka"         "127.0.0.1:9092"
check_grpc "StatusServer"  "127.0.0.1:50052"
check_grpc "MailerServer"  "127.0.0.1:50051"
check_grpc "UMSServer"     "127.0.0.1:50053"
check_grpc "TaskServer"    "127.0.0.1:50054"
check_grpc "PushServer1"   "127.0.0.1:8890"
check_grpc "PushServer1g"  "127.0.0.1:50056"
check_grpc "PushServer2"   "127.0.0.1:8891"
check_grpc "PushServer2g"  "127.0.0.1:50057"
check_http "GateServer"    "http://127.0.0.1:8080/user_login"
check_http "Client"        "http://127.0.0.1:3000"

echo ""
echo "=== Logs: $LOGDIR ==="
echo "To stop: bash script/stop_all.sh"
