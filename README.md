# OxyTasks

ACM 集训队分布式训练任务协调系统。基于 C++ 微服务 + Vue 3 前端 + MySQL/Redis 构建。

## 系统架构

```
Client (Vue 3, port 5173)
  │  HTTP POST (Axios, proxied via Vite to GateServer)
  ▼
GateServer (HTTP 网关, port 8080, Boost.Beast)
  │  gRPC
  ├── UMSServer (port 50053)       — 用户认证、注册、角色管理
  ├── TaskServer (port 50054)      — 任务、TODO、签到
  ├── StatusServer (port 50052)    — 在线状态、推送调度、日志聚合、监控
  ├── PushServer (WS:8890, gRPC:50056)  — WebSocket 推送节点 1
  └── PushServer2 (WS:8891, gRPC:50057) — WebSocket 推送节点 2
MailerServer (Node.js, port 50051) — 邮件验证码
```

## 目录结构

```
├── GateServer/          # HTTP 网关，前端请求唯一入口
├── UMSServer/           # 用户管理服务
├── TaskServer/          # 任务/TODO/签到服务
├── StatusServer/        # 状态服务，推送节点分配，心跳监控，日志聚合
├── PushServer/          # WebSocket 推送服务（节点 1）
├── PushServer2/         # WebSocket 推送服务（节点 2）
├── MailerServer/        # 邮件服务（Node.js）
├── Client/              # Vue 3 前端 SPA
├── docs/                # 设计文档与调试记录
├── build_all.sh         # 构建所有 C++ 服务
├── start_all.sh         # 启动所有服务
├── stop_all.sh          # 停止所有服务
├── clear_logs.sh        # 清理日志
├── user.sql             # 用户表
├── task.sql             # 任务表
├── task_assignments.sql # 任务分配表
├── todo_list.sql        # TODO 表
└── messages.sql         # 消息表
```

## 本地快速开始

### 环境依赖

- **C++ 服务**: C++17, CMake 3.16+, Boost (system, filesystem), protobuf, gRPC, hiredis, MySQL Connector/C++, jsoncpp
- **前端**: Node.js 18+, npm
- **数据库**: MySQL 8.0, Redis 6+

### 1. 初始化数据库

```bash
mysql -u root -p < user.sql
mysql -u root -p oxytasks < task.sql
mysql -u root -p oxytasks < task_assignments.sql
mysql -u root -p oxytasks < todo_list.sql
mysql -u root -p oxytasks < messages.sql
```

### 2. 修改配置

每个服务目录下的 `config.ini`（或 `config.json`）需配置 MySQL/Redis 连接信息。

### 3. 构建

```bash
# 构建所有 C++ 服务
./build_all.sh

# 构建前端
cd Client && npm install && npm run build
```

### 4. 启动

```bash
# 启动所有服务（按依赖顺序）
./start_all.sh

# 停止所有服务
./stop_all.sh
```

启动顺序：StatusServer → UMSServer → TaskServer → PushServer1 → PushServer2 → GateServer

### 5. 访问

- 前端开发服务器: `http://localhost:5173`（`cd Client && npm run dev`）
- GateServer HTTP: `http://localhost:8080`

## 角色系统

| 角色 | 值 | 权限 |
|---|---|---|
| 队员 (member) | 0 | 查看/完成分配的任务、个人 TODO、每日签到、查看消息 |
| 队长 (captain) | 1 | 队员权限 + 创建/编辑/删除任务、管理团队、查看团队进度 |
| 教练 (coach) | 2 | 队长权限 + 审批用户、管理所有队伍、系统监控、日志查看 |

新注册用户状态为"待审批"，需教练审核通过后方可使用。

## 功能模块

### 任务管理

- 队长/教练创建任务，支持多指派人、优先级（高/中/低）、截止日期
- 每个指派人独立跟踪状态（`task_assignments` 表）
- 实时推送通知：新任务 (`task_new`)、状态变更 (`task_update`)、任务完成 (`task_done`)、截止提醒 (`task_remind`)

### TODO 清单

- 个人待办事项，支持优先级、截止日期、完成状态
- 按优先级和截止日期排序

### 每日签到

- 每人每天一次，日历可视化
- 团队进度视图展示成员签到统计

### 消息中心

- 实时 WebSocket 推送 + 离线消息缓存
- 消息类型：任务通知、签到提醒、重复登录踢出
- 未读角标、标记已读、删除

### 系统监控（教练专属）

- 服务心跳状态
- 跨服务日志聚合查询
- PushServer 连接数统计

## API 规范

- 所有接口均为 `POST`
- 路径格式：`/<模块>_<动作>`（如 `/task_create`、`/todo_update`）
- 返回格式：`{ "error": 0, ... }`，`error=0` 表示成功
- 认证：`Authorization: Bearer <token>` + `X-User-Id` 请求头

### 主要接口

| 模块 | 接口 |
|---|---|
| 用户 | `/get_verify_code` `/user_register` `/user_resetpass` `/user_login` `/user_list_pending` `/user_approve` `/user_reject` `/user_set_role` `/user_list_all` |
| 任务 | `/task_create` `/task_update` `/task_delete` `/task_get` `/task_list` |
| TODO | `/todo_add` `/todo_list` `/todo_update` `/todo_delete` |
| 签到 | `/checkin` `/checkin_list` |
| 消息 | `/msg_list` `/msg_read` `/msg_delete` |
| 监控 | `/monitor/query_logs` `/monitor/server_status` |

## 推送架构

```
任务服务.创建任务
  └── PushGrpcClient.pushToUser()
        └── StatusServer.GetPushServerForUser(uid)
              └── 查 Redis pushnode:<uid> → 目标 PushServer
                    ├── 用户在本节点 → WebSocket 直推
                    └── 用户在其他节点 → gRPC 转发
```

- **负载均衡**: StatusServer 支持 SegmentTree（O(log n)）和 Brute（O(n)）两种分配算法
- **跨节点转发**: PushServer 自动通过 Redis 定位用户所在节点并转发
- **离线缓存**: 消息写入 Redis (`msgs:<uid>`, 最多 50 条, 7 天 TTL) + MySQL，登录时补推
- **重复登录**: 新登录踢出旧连接，发送 `kicked` 消息

## 技术栈

### 后端

| 组件 | 技术 |
|---|---|
| HTTP 网关 | C++17, Boost.Beast, Boost.Asio |
| RPC | gRPC, Protocol Buffers |
| 数据库 | MySQL 8.0, MySQL Connector/C++ |
| 缓存 | Redis, hiredis |
| 邮件 | Node.js, nodemailer |

### 前端

| 组件 | 技术 |
|---|---|
| 框架 | Vue 3 (Composition API) |
| 状态管理 | Pinia |
| UI 组件 | Element Plus |
| HTTP | Axios |
| 路由 | Vue Router 4 (Hash 模式) |
| 构建 | Vite 5 |

## 日志与调试

```bash
# 查看服务实时日志
tail -f TaskServer/logs/TaskServer_$(date +%Y-%m-%d).log

# 查看启动日志
tail -f server_logs/TaskServer.log

# 清理所有日志
./clear_logs.sh

# gRPC 直连测试
grpcurl -plaintext -d '{"uid":1}' localhost:50054 message.TaskService/ListTasks

# 完整链路测试
curl -X POST http://localhost:8080/task_list \
  -H "Content-Type: application/json" \
  -d '{"uid":0,"status":-1,"assigned_to":"0"}'
```

## License

MIT
