# 消息中心（Message Center）设计文档

**日期**: 2026-05-29
**状态**: 已批准
**方案**: 方案 A — PushServer 作为独立 gRPC 服务端

---

## 1. 概述

实现 OxyTeamTasks 的"消息中心"模块，通过 WebSocket 为客户端实时推送消息提醒，支持离线消息缓存与历史查询。

### 消息类型

| 类型标识 | 说明 | 触发源 |
|---------|------|--------|
| `task_new` | 新任务指派通知 | TaskServer.CreateTask |
| `task_update` | 任务状态变更（完成/打回/修改） | TaskServer.UpdateTask |
| `task_done` | 指派人完成任务 | TaskServer.UpdateTask (uid>0, status=2) |
| `task_remind` | 任务截止日期提醒（当日到期） | StatusServer 定时扫描 |
| `checkin_remind` | 每日打卡提醒 | StatusServer 定时扫描 |
| `kicked` | 同账号新登录挤下线 | StatusServer.AllocatePushServer |

### 架构选型

- PushServer 新增 `PushService` gRPC 服务端，接收来自 TaskServer 和 StatusServer 的推送请求
- TaskServer 通过 StatusServer 动态查询用户所在的 PushServer 节点（支持集群部署）
- 消息持久化：MySQL `messages` 表 + Redis 缓存（未读计数 + 近期消息 List）
- StatusServer 负责定时扫描任务到期和未打卡用户

---

## 2. 数据模型

### MySQL `messages` 表

```sql
CREATE TABLE messages (
    id BIGINT AUTO_INCREMENT PRIMARY KEY,
    uid INT NOT NULL,                    -- 接收者 UID
    type VARCHAR(32) NOT NULL,           -- 消息类型标识
    title VARCHAR(255) NOT NULL,         -- 消息标题
    content TEXT,                        -- 消息正文（JSON payload）
    is_read TINYINT DEFAULT 0,           -- 0=未读 1=已读
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    INDEX idx_uid_read (uid, is_read),
    INDEX idx_uid_created (uid, created_at DESC)
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
```

### Redis 缓存结构

| Key | 类型 | 说明 | TTL |
|-----|------|------|-----|
| `unread:<uid>` | STRING (INT) | 未读消息计数 | 无（主动管理） |
| `msgs:<uid>` | LIST | 近期消息缓存（LPUSH，LTRIM 50 条） | 7 天 |
| `pushnode:<uid>` | STRING | 用户所在的 PushServer 节点 `host:grpc_port` | 1 天（与 Token 同步） |
| `online:<uid>` | STRING | 用户在线状态标记 | 300 秒（心跳续期） |

---

## 3. Proto 定义

在所有 `message.proto` 中新增以下内容（仅 PushServer 实现为 gRPC 服务端）：

```protobuf
// ==========================================
// 推送服务（PushServer）
// ==========================================

service PushService {
  rpc PushToUser(PushToUserReq) returns (PushToUserRsp);
  rpc PushToTeam(PushToTeamReq) returns (PushToTeamRsp);
  rpc GetMessages(GetMessagesReq) returns (GetMessagesRsp);
  rpc MarkRead(MarkReadReq) returns (MarkReadRsp);
  rpc DeleteMessage(DeleteMessageReq) returns (DeleteMessageRsp);
}

message PushToUserReq {
  int32 uid = 1;
  string msg_type = 2;
  string title = 3;
  string payload = 4;
}

message PushToUserRsp {
  int32 error = 1;
  bool delivered = 2;
}

message PushToTeamReq {
  int32 team_id = 1;
  string msg_type = 2;
  string title = 3;
  string payload = 4;
  int32 exclude_uid = 5;
}

message PushToTeamRsp {
  int32 error = 1;
  int32 delivered_count = 2;
  int32 cached_count = 3;
}

message GetMessagesReq {
  int32 uid = 1;
  int32 page = 2;
  int32 page_size = 3;
}

message MessageItem {
  int64 id = 1;
  string msg_type = 2;
  string title = 3;
  string content = 4;
  int32 is_read = 5;
  string created_at = 6;
}

message GetMessagesRsp {
  int32 error = 1;
  repeated MessageItem messages = 2;
  int32 unread_count = 3;
  int32 total = 4;
}

message MarkReadReq {
  int32 uid = 1;
  repeated int64 ids = 2;
}

message MarkReadRsp {
  int32 error = 1;
}

message DeleteMessageReq {
  int32 uid = 1;
  repeated int64 ids = 2;
}

message DeleteMessageRsp {
  int32 error = 1;
}
```

在 `StatusService` 中新增：

```protobuf
rpc GetPushServerForUser(GetPushServerForUserReq) returns (GetPushServerForUserRsp);

message GetPushServerForUserReq {
  repeated int32 uids = 1;
}

message UserPushNode {
  int32 uid = 1;
  string host = 2;
  string port = 3;
  bool online = 4;
}

message GetPushServerForUserRsp {
  int32 error = 1;
  repeated UserPushNode nodes = 2;
}
```

---

## 4. PushServer 改动

### 4.1 Session 增加 uid 字段

```cpp
// Session.h — 新增
private:
    int uid_ = 0;
public:
    void setUid(int uid) { uid_ = uid; }
    int getUid() const { return uid_; }
```

### 4.2 MainServer 增加 uid→session 映射

```cpp
// MainServer.h — 新增
private:
    std::unordered_map<int, std::shared_ptr<Session>> uid_sessions_;
    std::shared_mutex uid_mtx_;
public:
    void addUidSession(int uid, std::shared_ptr<Session> session);
    void removeUidSession(int uid);
    std::shared_ptr<Session> getSessionByUid(int uid);
```

- `loginHandler` 成功后调用 `addUidSession(uid, session)` + `session->setUid(uid)`
- `clearSession` 中如果 `session->getUid() > 0` 则调用 `removeUidSession(uid)`

### 4.3 PushGrpcServiceImpl（新增）

实现 `PushService` 的 5 个 RPC：

**PushToUser 核心逻辑**：
1. 写 MySQL `messages` 表
2. LPUSH 到 Redis `msgs:<uid>`，LTRIM 50 条，EXPIRE 7 天
3. INCR `unread:<uid>`
4. 通过 `MainServer::getSessionByUid(uid)` 查找在线 session
5. 如果在线：构造 `{"type":"notify","id":...,"msg_type":...,"title":...}` JSON → `session->send()`，返回 `delivered=true`
6. 如果离线：返回 `delivered=false`（消息已缓存在 Redis+MySQL）

**PushToTeam**：查 MySQL `user` 表获取 `team_id` 下所有 uid，逐个调用 PushToUser 逻辑（跳过 `exclude_uid`）。

**GetMessages**：分页查询 MySQL `messages` 表，同时返回 `unread_count`（从 Redis `unread:<uid>` 读取）。

**MarkRead**：更新 MySQL `is_read=1`，DECR Redis `unread:<uid>`。

**DeleteMessage**：DELETE FROM MySQL，不操作 Redis（Redis List 会在 TTL 后自然过期）。

### 4.4 gRPC 服务启动

在 `PushServerMain.cpp` 中新增 gRPC 服务线程，监听端口 `50055`（从 config.ini 读取）。

### 4.5 loginHandler 改动

Token 验证成功后：
1. `session->setUid(uid)`
2. `server->addUidSession(uid, session)`
3. SETEX `online:<uid>` = "1"，TTL 300 秒
4. 查询 `unread:<uid>` 返回未读计数
5. LRANGE `msgs:<uid>` 0 19 拉取最近 20 条离线消息
6. 在 `login_rsp` 中附带 `unread_count` 和 `offline_messages` 数组

### 4.6 clearSession 改动

```cpp
void MainServer::clearSession(std::string uuid) {
    auto it = sessions_.find(uuid);
    int uid = 0;
    if (it != sessions_.end()) {
        uid = it->second->getUid();
        sessions_.erase(it);
    }
    if (uid > 0) {
        removeUidSession(uid);
        RedisManager::getInstance().del("online:" + std::to_string(uid));
    }
    // ... 原有的 reportDisconnect 逻辑
}
```

---

## 5. TaskServer 改动

### 5.1 新增 PushGrpcClient

```cpp
class PushGrpcClient : public Singleton<PushGrpcClient> {
public:
    PushToUserRsp pushToUser(int uid, const std::string& msgType,
                             const std::string& title, const std::string& payload);
    PushToTeamRsp pushToTeam(int teamId, const std::string& msgType,
                             const std::string& title, const std::string& payload, int excludeUid);
private:
    // 内部先调用 StatusServer.GetPushServerForUser 获取目标 PushServer 地址
    // 再调用对应 PushServer 的 PushToUser/PushToTeam
};
```

### 5.2 业务触发点（异步 AsyncTaskPool）

| RPC | 触发条件 | 推送 |
|-----|---------|------|
| CreateTask | 成功 | `task_new` → assigned_to 中每个 uid |
| UpdateTask | uid=0, 全局状态变更 | `task_update` → 创建者 + 所有指派人 |
| UpdateTask | uid>0, status=2 | `task_done` → 创建者 |
| UpdateTask | uid>0, status=1 (打回) | `task_update` → 被打回的指派人 |
| DeleteTask | 成功 | `task_update` → 创建者 + 所有指派人 |

---

## 6. StatusServer 改动

### 6.1 AllocatePushServer 增加节点映射

分配成功后写入 Redis：
```cpp
std::string node_key = "pushnode:" + std::to_string(uid);
RedisManager::getInstance().setex(node_key, server.host + ":" + server.port, 86400);
```

### 6.2 新增 GetPushServerForUser RPC

批量查询 Redis `pushnode:<uid>` 返回每个用户所在的 PushServer 节点信息。

### 6.3 定时扫描

在心跳定时器基础上新增两个定时任务：

**任务截止日期提醒**（每天 08:00）：
```sql
SELECT id, uid, title, assigned_to FROM task
WHERE DATE(deadline) = CURDATE() AND status != 2
```
对每个任务的 `assigned_to` 中的 uid 调用 `PushToUser(uid, "task_remind", "任务今日到期", ...)`。

**每日打卡提醒**（每天 20:00）：
```sql
SELECT uid FROM user WHERE status = 1
AND uid NOT IN (SELECT uid FROM checkins WHERE checkin_date = CURDATE())
```
对每个未打卡用户调用 `PushToUser(uid, "checkin_remind", "今日尚未打卡", ...)`。

可在 `config.ini` 中配置提醒时间：
```ini
[Reminder]
deadline_hour = 8
checkin_hour = 20
```

---

## 7. GateServer 改动

### 7.1 新增 HTTP 路由

| 路由 | 说明 | 实现方式 |
|------|------|---------|
| `POST /msg_list` | 查询消息列表（分页） | GateServer 直连 MySQL |
| `POST /msg_read` | 标记已读 | GateServer 直连 MySQL + Redis |
| `POST /msg_delete` | 删除消息 | GateServer 直连 MySQL + Redis |

**设计决策**：`messages` 表是全局 MySQL 表，不绑定特定 PushServer 节点。所有消息 CRUD 操作由 GateServer 直连 MySQL 和 Redis 完成，无需经过 PushServer gRPC。这样 GateServer 不需要新增 PushGrpcClient，简化架构。

- `/msg_list`：分页查询 MySQL，同时读 Redis `unread:<uid>` 返回未读计数
- `/msg_read`：UPDATE MySQL `is_read=1`，DECR Redis `unread:<uid>`
- `/msg_delete`：DELETE FROM MySQL（Redis List 会自然过期）

---

## 8. 前端改动

### 8.1 新增 `Client/src/api/message.js`

```js
import request from './request'
export const getMessages = (params) => request.post('/msg_list', params)
export const markRead = (data) => request.post('/msg_read', data)
export const deleteMessage = (data) => request.post('/msg_delete', data)
```

### 8.2 MainLayout — 通知铃 + 消息面板

- 将 `el-badge :value="0"` 改为动态 `:value="unreadCount"`
- 点击铃铛展开消息下拉面板（最近 10 条）
- 每条显示标题 + 时间，未读加粗
- 底部"查看全部消息"链接跳转 `/messages`
- "全部已读"按钮

### 8.3 pushClient.js — 注册消息 handler

```js
onMessage('notify', (data) => { /* unreadCount++, ElNotification 弹窗 */ })
onMessage('kicked', (data) => { /* 清除 localStorage, 跳转 /login */ })
// login_rsp handler 扩展：读取 unread_count 和 offline_messages
```

### 8.4 MessagesView — 消息中心页面

- el-table 分页展示消息列表
- 消息类型标签颜色区分
- 操作列：标记已读、删除
- 顶部统计：未读 X 条

---

## 9. 涉及文件清单

| 模块 | 文件 | 改动类型 |
|------|------|---------|
| Proto | `*/message.proto` (×6) | 新增 PushService + GetPushServerForUser |
| PushServer | `Session.h/.cpp` | 新增 uid 字段 |
| PushServer | `MainServer.h/.cpp` | 新增 uid→session 映射 |
| PushServer | `PushGrpcServiceImpl.h/.cpp` | 新建，实现 5 个 RPC |
| PushServer | `PushServerMain.cpp` | 启动 gRPC 服务 |
| PushServer | `LogicSystem.cpp` | loginHandler 增加 uid 映射 + 离线消息拉取 |
| PushServer | `Global.h` | 新增 WS_MSG_NOTIFY 等消息类型常量 |
| PushServer | `config.ini` | 新增 gRPC 端口配置 |
| PushServer | `CMakeLists.txt` | 新增 PushGrpcServiceImpl 源文件 |
| TaskServer | `PushGrpcClient.h/.cpp` | 新建 |
| TaskServer | `TaskGrpcServiceImpl.cpp` | 各 RPC 增加异步推送调用 |
| TaskServer | `CMakeLists.txt` | 新增 PushGrpcClient 源文件 |
| TaskServer | `config.ini` | 新增 StatusServer 地址（如未有） |
| StatusServer | `StatusServiceImpl.cpp` | AllocatePushServer 写 pushnode；新增 GetPushServerForUser |
| StatusServer | `StatusServiceImpl.h` | 新增方法声明 |
| StatusServer | `StatusServer.cpp` | 新增定时扫描逻辑 |
| StatusServer | `config.ini` | 新增提醒时间配置 |
| GateServer | `LogicSystem.cpp` | 新增 /msg_list, /msg_read, /msg_delete 路由（直连 MySQL + Redis） |
| Client | `api/message.js` | 新建 |
| Client | `views/messages/MessagesView.vue` | 重写 |
| Client | `layout/MainLayout.vue` | 通知铃 + 消息面板 |
| Client | `utils/pushClient.js` | 注册 notify/kicked handler |

---

## 10. 实施阶段建议

| 阶段 | 内容 | 涉及模块 |
|------|------|---------|
| P1 基础设施 | MySQL 建表、Proto 定义（×6）、PushServer uid→session 映射、PushGrpcServiceImpl 骨架 | Proto, PushServer |
| P2 推送通道 | PushGrpcServiceImpl 完整实现、TaskServer PushGrpcClient + 异步推送触发、StatusServer GetPushServerForUser + AllocatePushServer 写 pushnode | PushServer, TaskServer, StatusServer |
| P3 消息 CRUD | GateServer /msg_list, /msg_read, /msg_delete 路由 | GateServer |
| P4 定时提醒 | StatusServer 定时扫描（截止日期 + 未打卡） | StatusServer |
| P5 前端 | api/message.js、MainLayout 通知铃 + 消息面板、MessagesView、pushClient.js handler | Client |
| P6 联调 | 端到端测试：创建任务→推送→通知铃→消息中心 | 全栈 |
