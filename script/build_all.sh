#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
SERVERS=("GateServer" "UMSServer" "StatusServer" "PushServer" "PushServer2" "TaskServer")

GREEN='\033[0;32m'
RED='\033[0;31m'
YELLOW='\033[1;33m'
NC='\033[0m'

echo -e "${YELLOW}Building all servers...${NC}"
echo "============================"

FAILED=()

for server in "${SERVERS[@]}"; do
    BUILD_DIR="$PROJECT_DIR/$server/build"
    mkdir -p "$BUILD_DIR"
    echo ""
    echo -e "${YELLOW}[$server]${NC} Configuring..."

    if ! cmake -S "$PROJECT_DIR/$server" -B "$BUILD_DIR" > "$BUILD_DIR/cmake.log" 2>&1; then
        echo -e "${RED}[$server] CMake configure FAILED${NC}"
        cat "$BUILD_DIR/cmake.log"
        FAILED+=("$server (configure)")
        continue
    fi

    echo -e "${YELLOW}[$server]${NC} Building..."
    if cmake --build "$BUILD_DIR" -j$(nproc) > "$BUILD_DIR/build.log" 2>&1; then
        echo -e "${GREEN}[$server] Build OK${NC}"
    else
        echo -e "${RED}[$server] Build FAILED${NC}"
        tail -20 "$BUILD_DIR/build.log"
        FAILED+=("$server (build)")
    fi
done

echo ""
echo "============================"
if [ ${#FAILED[@]} -eq 0 ]; then
    echo -e "${GREEN}All ${#SERVERS[@]} servers built successfully.${NC}"
else
    echo -e "${RED}${#FAILED[@]} server(s) failed:${NC}"
    for f in "${FAILED[@]}"; do
        echo -e "  ${RED}✗${NC} $f"
    done
    exit 1
fi
