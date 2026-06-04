#!/bin/bash
# 一键清除所有服务器 logs 文件夹内容
# 用法: bash script/clear_logs.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

LOGS_DIRS=(
    "$PROJECT_DIR/GateServer/logs"
    "$PROJECT_DIR/GateServer/build/logs"
    "$PROJECT_DIR/PushServer/logs"
    "$PROJECT_DIR/PushServer/build/logs"
    "$PROJECT_DIR/PushServer2/logs"
    "$PROJECT_DIR/PushServer2/build/logs"
    "$PROJECT_DIR/StatusServer/logs"
    "$PROJECT_DIR/StatusServer/build/logs"
    "$PROJECT_DIR/UMSServer/logs"
    "$PROJECT_DIR/UMSServer/build/logs"
    "$PROJECT_DIR/TaskServer/logs"
    "$PROJECT_DIR/TaskServer/build/logs"
)

echo "=== Clearing server logs ==="
cleared=0

for dir in "${LOGS_DIRS[@]}"; do
    if [ -d "$dir" ]; then
        count=$(ls -1 "$dir" 2>/dev/null | wc -l)
        if [ "$count" -gt 0 ]; then
            rm -f "$dir"/*
            echo "  $dir — removed $count file(s)"
            cleared=$((cleared + 1))
        else
            echo "  $dir — already empty"
        fi
    else
        echo "  $dir — not found, skipping"
    fi
done

echo "=== Done: $cleared directories cleared ==="
