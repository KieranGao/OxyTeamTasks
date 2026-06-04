#!/bin/bash
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BASE="$(cd "$SCRIPT_DIR/.." && pwd)"
LOGDIR="$BASE/server_logs"
mkdir -p "$LOGDIR"

echo "=== Starting all servers ==="

# 1. StatusServer (依赖项，先启动)
echo "  [1/6] StatusServer (port 50052)..."
cd "$BASE/StatusServer/build"
./StatusServer > "$LOGDIR/StatusServer.log" 2>&1 &
sleep 1

# 2. UMSServer
echo "  [2/6] UMSServer (port 50051)..."
cd "$BASE/UMSServer/build"
./UMSServer > "$LOGDIR/UMSServer.log" 2>&1 &
sleep 1

# 3. TaskServer
echo "  [3/6] TaskServer (port 50054)..."
cd "$BASE/TaskServer/build"
./TaskServer > "$LOGDIR/TaskServer.log" 2>&1 &
sleep 1

# 4. PushServer1
echo "  [4/6] PushServer1 (ws:8890 grpc:50056)..."
cd "$BASE/PushServer/build"
./PushServer > "$LOGDIR/PushServer.log" 2>&1 &
sleep 1

# 5. PushServer2
echo "  [5/6] PushServer2 (ws:8891 grpc:50057)..."
cd "$BASE/PushServer2/build"
./PushServer2 > "$LOGDIR/PushServer2.log" 2>&1 &
sleep 1

# 6. GateServer
echo "  [6/6] GateServer (port 8080)..."
cd "$BASE/GateServer/build"
./GateServer > "$LOGDIR/GateServer.log" 2>&1 &
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

check_grpc "StatusServer"  "127.0.0.1:50052"
check_grpc "UMSServer"     "127.0.0.1:50053"
check_grpc "TaskServer"    "127.0.0.1:50054"
check_grpc "PushServer1"   "127.0.0.1:8890"
check_grpc "PushServer1g"  "127.0.0.1:50056"
check_grpc "PushServer2"   "127.0.0.1:8891"
check_grpc "PushServer2g"  "127.0.0.1:50057"
check_http "GateServer"    "http://127.0.0.1:8080/user_login"

echo ""
echo "=== Logs: $LOGDIR ==="
echo "To stop: bash script/stop_all.sh"
