# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OxyTeamTask is a distributed training task coordination system for ACM competitive programming teams. Backend is 5 C++ microservices + 1 Node.js mailer, frontend is Vue 3 SPA, database is MySQL 8.0 + Redis.

## Build & Run Commands

```bash
# Build all 5 C++ servers
bash script/build_all.sh

# Build individual server (example: TaskServer)
cd TaskServer && cmake -B build -S . && cmake --build build

# Build frontend
cd Client && npm run build    # production
cd Client && npm run dev      # dev server at localhost:5173

# Start/stop all servers (start order: StatusServer → UMSServer → TaskServer → PushServer → GateServer)
bash script/start_all.sh
bash script/stop_all.sh

# Clear all server logs
bash script/clear_logs.sh
```

## Architecture

```
Client (Vue 3, port 5173)
  │  HTTP POST (Axios, proxied via Vite to GateServer)
  ▼
GateServer (HTTP gateway, port 8080, Boost.Beast)
  │  gRPC
  ├── UMSServer (port 50053)       — user auth, registration, roles
  ├── TaskServer (port 50054)      — tasks, TODOs, check-ins
  ├── StatusServer (port 50052)    — online status, push scheduling, logging
  └── PushServer (port 8890)       — WebSocket push to clients (Boost.Asio)
MailerServer (Node.js)             — email verification via gRPC
```

GateServer is the HTTP→gRPC gateway. All frontend requests go through it. It translates JSON HTTP POST to protobuf gRPC calls. Each downstream server has its own `TaskGrpcClient`/`UserGrpcClient`/`StatusGrpcClient` in GateServer.

Proto files are **not shared** — each server directory contains its own `message.proto` with the subset of services it needs. All 5 C++ servers' protos must stay in sync (identical content). Generated `.pb.h/.pb.cc` files are placed alongside `.proto` files (via CMake `CMAKE_CURRENT_SOURCE_DIR`).

## Role System

- `role=0`: member (队员) — can view/complete assigned tasks, personal TODOs, check-in
- `role=1`: captain (队长) — member powers + create tasks, manage team, view team progress
- `role=2`: coach (教练) — captain powers + approve users, manage all teams, system monitor

Frontend route guards enforce roles via `meta.roles` in `router/index.js`. Role mapping: `{ 0:'member', 1:'captain', 2:'coach' }`.

## Key Data Patterns

**Task assignments are independent per user**: The `task_assignments` table stores per-assignee status (`task_id, assignee_uid, status`). The `tasks` table `status` field is the global/shared status. When displaying a specific user's task status, always consult `assignee_statuses` (populated via batch SQL) — never trust the global `t.status` for multi-assignee tasks.

**API endpoint naming**: All HTTP endpoints are POST, path format `/<module>_<action>` (e.g., `/task_create`, `/todo_update`, `/checkin_list`).

**API return convention**: All responses have `{ error: 0 }` for success. Error codes are integers: 0=success, 1001=JSON parse error, 1002=RPC error, 3001=already checked in today.

## Adding a New Feature (Server-side)

Follow this order (from `docs/后续的开发路线.md`):

1. Add RPC + messages to `message.proto` (in both TaskServer and GateServer)
2. Add data struct to `Global.h` if needed
3. Implement `MySQLDao` method (SQL layer)
4. Add `MySQLManager` delegation (thin singleton wrapper)
5. Implement `TaskGrpcServiceImpl` handler
6. Add `TaskGrpcClient` method in GateServer
7. Register HTTP route in `LogicSystem.cpp`
8. Build both servers → test with curl

## Code Conventions

- **Every RPC must have entry/exit logging**: `LOG_INFO` with key parameters at entry, result at exit
- **gRPC failures must log `status.error_message()`**: Don't silently set error code without logging
- **Never re-lock if caller already holds the lock**: Internal methods called within a locked section should not acquire `lock_guard` again (deadlock lesson from 2026-05-28)
- **Proto fields: append only, never modify**: Changing field numbers/types breaks wire compatibility
- **Frontend status display**: Use `assignee_statuses` array from the API, not the global task status, for per-user views. The `my_status` field (populated via LEFT JOIN in backend) is available for single-user queries.
- **Client-side filtering**: `listTasks({ uid: 0 })` returns all tasks. Views that need "assigned to me" must filter client-side with `assignedUids.includes(uid)`. Views that need "created by me" must filter with `t.uid === myUid`.

## Testing Flow

```bash
# 1. Check service is running
ps aux | grep TaskServer

# 2. Direct gRPC call (bypass GateServer)
grpcurl -plaintext -d '{"uid":1}' localhost:50054 message.TaskService/ListTasks

# 3. Full chain: HTTP → GateServer → gRPC → TaskServer
curl -X POST http://localhost:8080/task_list \
  -H "Content-Type: application/json" \
  -d '{"uid":0,"status":-1,"assigned_to":"0"}'

# 4. Check logs
tail -f TaskServer/logs/TaskServer_$(date +%Y-%m-%d).log
```

## Database

MySQL database: `oxytasks`. Schema files are at the project root:
- `user.sql`, `task.sql`, `task_assignments.sql`, `todo_list.sql`, `checkins` (created inline)

MySQL config: `config.ini` in each server directory. Connection pool size: 5 (DAO) / 8 (RPC).

## Frontend

- Vue 3 Composition API (`<script setup>`) throughout
- Pinia for state (`stores/user.js` for auth, persisted to localStorage)
- Element Plus for UI components, auto-imported icons via `@element-plus/icons-vue`
- Axios instance in `api/request.js` — auto-attaches `Authorization` and `X-User-Id` headers
- Hash-based routing (`createWebHashHistory`) for Electron compatibility
- WebSocket push client in `utils/pushClient.js` with auto-reconnect


Behavioral guidelines to reduce common LLM coding mistakes. Merge with project-specific instructions as needed.

Tradeoff: These guidelines bias toward caution over speed. For trivial tasks, use judgment.

1. Think Before Coding
Don't assume. Don't hide confusion. Surface tradeoffs.

Before implementing:

State your assumptions explicitly. If uncertain, ask.
If multiple interpretations exist, present them - don't pick silently.
If a simpler approach exists, say so. Push back when warranted.
If something is unclear, stop. Name what's confusing. Ask.
2. Simplicity First
Minimum code that solves the problem. Nothing speculative.

No features beyond what was asked.
No abstractions for single-use code.
No "flexibility" or "configurability" that wasn't requested.
No error handling for impossible scenarios.
If you write 200 lines and it could be 50, rewrite it.
Ask yourself: "Would a senior engineer say this is overcomplicated?" If yes, simplify.

3. Surgical Changes
Touch only what you must. Clean up only your own mess.

When editing existing code:

Don't "improve" adjacent code, comments, or formatting.
Don't refactor things that aren't broken.
Match existing style, even if you'd do it differently.
If you notice unrelated dead code, mention it - don't delete it.
When your changes create orphans:

Remove imports/variables/functions that YOUR changes made unused.
Don't remove pre-existing dead code unless asked.
The test: Every changed line should trace directly to the user's request.

4. Goal-Driven Execution
Define success criteria. Loop until verified.

Transform tasks into verifiable goals:

"Add validation" → "Write tests for invalid inputs, then make them pass"
"Fix the bug" → "Write a test that reproduces it, then make it pass"
"Refactor X" → "Ensure tests pass before and after"
For multi-step tasks, state a brief plan:

1. [Step] → verify: [check]
2. [Step] → verify: [check]
3. [Step] → verify: [check]
Strong success criteria let you loop independently. Weak criteria ("make it work") require constant clarification.

These guidelines are working if: fewer unnecessary changes in diffs, fewer rewrites due to overcomplication, and clarifying questions come before implementation rather than after mistakes.