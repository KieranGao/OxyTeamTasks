[中文](README.md) | [English](README_EN.md)

# OxyTasks

Distributed training task coordination system for ACM competitive programming teams

![C++](https://img.shields.io/badge/C++17-00599C?style=flat&logo=cplusplus&logoColor=white)
![Vue](https://img.shields.io/badge/Vue_3-4FC08D?style=flat&logo=vuedotjs&logoColor=white)
![gRPC](https://img.shields.io/badge/gRPC-244c5a?style=flat&logo=grpc&logoColor=white)
![MySQL](https://img.shields.io/badge/MySQL-4479A1?style=flat&logo=mysql&logoColor=white)
![Redis](https://img.shields.io/badge/Redis-DC382D?style=flat&logo=redis&logoColor=white)

## Architecture

<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 820 320">
  <defs>
    <marker id="arrowhead" markerWidth="10" markerHeight="7" refX="10" refY="3.5" orient="auto">
      <polygon points="0 0, 10 3.5, 0 7" fill="#606266"/>
    </marker>
  </defs>

  <!-- Client -->
  <rect x="20" y="120" width="120" height="60" rx="8" fill="#4A90D9"/>
  <text x="80" y="145" text-anchor="middle" fill="white" font-size="14" font-weight="bold">Client</text>
  <text x="80" y="165" text-anchor="middle" fill="white" font-size="11">Vue 3 SPA :5173</text>

  <!-- GateServer -->
  <rect x="200" y="120" width="130" height="60" rx="8" fill="#E6A23C"/>
  <text x="265" y="145" text-anchor="middle" fill="white" font-size="14" font-weight="bold">GateServer</text>
  <text x="265" y="165" text-anchor="middle" fill="white" font-size="11">HTTP :8080</text>

  <!-- UMSServer -->
  <rect x="400" y="20" width="130" height="55" rx="8" fill="#67C23A"/>
  <text x="465" y="43" text-anchor="middle" fill="white" font-size="13" font-weight="bold">UMSServer</text>
  <text x="465" y="62" text-anchor="middle" fill="white" font-size="10">gRPC :50053</text>

  <!-- TaskServer -->
  <rect x="400" y="90" width="130" height="55" rx="8" fill="#67C23A"/>
  <text x="465" y="113" text-anchor="middle" fill="white" font-size="13" font-weight="bold">TaskServer</text>
  <text x="465" y="132" text-anchor="middle" fill="white" font-size="10">gRPC :50054</text>

  <!-- StatusServer -->
  <rect x="400" y="160" width="130" height="55" rx="8" fill="#67C23A"/>
  <text x="465" y="183" text-anchor="middle" fill="white" font-size="13" font-weight="bold">StatusServer</text>
  <text x="465" y="202" text-anchor="middle" fill="white" font-size="10">gRPC :50052</text>

  <!-- PushServer x2 -->
  <rect x="400" y="230" width="60" height="55" rx="8" fill="#67C23A"/>
  <text x="430" y="253" text-anchor="middle" fill="white" font-size="12" font-weight="bold">Push</text>
  <text x="430" y="272" text-anchor="middle" fill="white" font-size="9">WS:8890</text>

  <rect x="470" y="230" width="60" height="55" rx="8" fill="#67C23A"/>
  <text x="500" y="253" text-anchor="middle" fill="white" font-size="12" font-weight="bold">Push2</text>
  <text x="500" y="272" text-anchor="middle" fill="white" font-size="9">WS:8891</text>

  <!-- MailerServer -->
  <rect x="590" y="20" width="100" height="55" rx="8" fill="#67C23A"/>
  <text x="640" y="43" text-anchor="middle" fill="white" font-size="13" font-weight="bold">Mailer</text>
  <text x="640" y="62" text-anchor="middle" fill="white" font-size="10">Node.js</text>

  <!-- MySQL -->
  <rect x="590" y="120" width="100" height="55" rx="8" fill="#909399"/>
  <text x="640" y="143" text-anchor="middle" fill="white" font-size="13" font-weight="bold">MySQL</text>
  <text x="640" y="162" text-anchor="middle" fill="white" font-size="10">8.0 :3306</text>

  <!-- Redis -->
  <rect x="710" y="120" width="100" height="55" rx="8" fill="#909399"/>
  <text x="760" y="143" text-anchor="middle" fill="white" font-size="13" font-weight="bold">Redis</text>
  <text x="760" y="162" text-anchor="middle" fill="white" font-size="10">:6379</text>

  <!-- Arrows: Client -> GateServer -->
  <line x1="140" y1="150" x2="200" y2="150" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <text x="170" y="143" text-anchor="middle" fill="#606266" font-size="9">HTTP</text>

  <!-- GateServer -> services -->
  <line x1="330" y1="135" x2="400" y2="50" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="330" y1="145" x2="400" y2="115" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="330" y1="155" x2="400" y2="185" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="330" y1="165" x2="400" y2="255" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>

  <!-- Services -> Data -->
  <line x1="530" y1="50" x2="590" y2="50" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="530" y1="115" x2="590" y2="145" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="530" y1="185" x2="590" y2="145" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
  <line x1="690" y1="145" x2="710" y2="145" stroke="#606266" stroke-width="2" marker-end="url(#arrowhead)"/>
</svg>

## Features

- **User Management**: Registration (email verification), login, role system (member/captain/coach), coach approval, team assignment
- **Task Management**: Multi-assignee tasks, independent status tracking, priority & deadlines, real-time WebSocket push notifications
- **TODO List**: Personal to-do items, priority sorting, deadline management
- **Daily Check-in**: Calendar visualization, team check-in statistics
- **Message Center**: Real-time push + offline caching, unread badge, mark as read, cross-node forwarding
- **System Monitoring**: Service heartbeat detection, cross-service log aggregation, PushServer connection stats (coach only)
- **Push Architecture**: Multi-PushServer load balancing, SegmentTree allocation algorithm, cross-node gRPC forwarding, offline message catch-up

## Quick Start

### Prerequisites

- **C++ Servers**: C++17, CMake 3.16+, Boost (system/filesystem), protobuf, gRPC, hiredis, MySQL Connector/C++, jsoncpp
- **Frontend**: Node.js 18+, npm
- **Database**: MySQL 8.0, Redis 6+

### 1. Initialize Database

```bash
mysql -u root -p < user.sql
mysql -u root -p oxytasks < task.sql
mysql -u root -p oxytasks < task_assignments.sql
mysql -u root -p oxytasks < todo_list.sql
mysql -u root -p oxytasks < messages.sql
```

### 2. Configure

Each server directory contains a `config.ini` (or `config.json` for MailerServer) where you set MySQL/Redis connection details.

### 3. Build

```bash
# Build all C++ servers
./build_all.sh

# Install frontend dependencies
cd Client && npm install
```

### 4. Start

```bash
# Start all servers (in dependency order)
./start_all.sh

# Stop all servers
./stop_all.sh
```

Startup order: StatusServer -> UMSServer -> TaskServer -> PushServer -> PushServer2 -> GateServer

### 5. Access

- Frontend dev server: `http://localhost:5173` (`cd Client && npm run dev`)
- GateServer HTTP: `http://localhost:8080`

## Directory Structure

```
OxyTasks/
├── GateServer/          # HTTP gateway, single entry point for frontend (C++17, Boost.Beast)
├── UMSServer/           # User management service (auth/roles/approval)
├── TaskServer/          # Task/TODO/Check-in service
├── StatusServer/        # Status scheduling service (push allocation/heartbeat/log aggregation)
├── PushServer/          # WebSocket push node 1 (Boost.Asio)
├── PushServer2/         # WebSocket push node 2
├── MailerServer/        # Email service (Node.js, nodemailer)
├── Client/              # Vue 3 SPA frontend
├── jsoncpp/             # JSON parsing library source
├── docs/                # Design documents and debug records
├── build_all.sh         # Build all C++ servers at once
├── start_all.sh         # Start all servers
├── stop_all.sh          # Stop all servers
├── clear_logs.sh        # Clear logs
├── user.sql             # User table schema
├── task.sql             # Task table schema
├── task_assignments.sql # Task assignment table schema
├── todo_list.sql        # TODO table schema
└── messages.sql         # Message table schema
```

## Tech Stack

### Backend

| Component | Technology |
|---|---|
| HTTP Gateway | C++17, Boost.Beast, Boost.Asio |
| RPC | gRPC, Protocol Buffers |
| Database | MySQL 8.0, MySQL Connector/C++ |
| Cache | Redis 6+, hiredis |
| JSON Parsing | jsoncpp |
| Email Service | Node.js, @grpc/grpc-js, nodemailer, ioredis |
| Logging | Custom Logger (file + buffered flush) |

### Frontend

| Component | Technology |
|---|---|
| Framework | Vue 3 (Composition API, `<script setup>`) |
| State Management | Pinia (persisted to localStorage) |
| UI Components | Element Plus, @element-plus/icons-vue |
| HTTP Client | Axios (auto-attaches Authorization/X-User-Id headers) |
| Router | Vue Router 4 (Hash mode) |
| Build Tool | Vite 5 |
| Push Client | WebSocket (auto-reconnect) |

## API Specification

- All endpoints are `POST`
- Path format: `/<module>_<action>` (e.g., `/task_create`, `/todo_update`)
- Response format: `{ "error": 0, ... }`, `error=0` means success
- Auth: `Authorization: Bearer <token>` + `X-User-Id` headers

### Endpoints

| Module | Endpoints |
|---|---|
| User | `/get_verify_code` `/user_register` `/user_resetpass` `/user_login` `/user_list_pending` `/user_approve` `/user_reject` `/user_set_role` `/user_list_all` |
| Task | `/task_create` `/task_update` `/task_delete` `/task_get` `/task_list` |
| TODO | `/todo_add` `/todo_list` `/todo_update` `/todo_delete` |
| Check-in | `/checkin` `/checkin_list` |
| Message | `/msg_list` `/msg_read` `/msg_delete` |
| Monitor | `/monitor/query_logs` `/monitor/server_status` |

## Contributing

Contributions are welcome! Please follow these steps:

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'feat: add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Create a Pull Request

## License

This project is licensed under the [MIT](LICENSE) License.
