# Redis 并发安全改造设计文档

**日期**: 2026-05-31  
**状态**: 待审核  
**作者**: OxyTheCrack + Claude  

---

## 一、背景与问题

### 1.1 现状

OxyTasks 是一个分布式 ACM 团队任务协调系统，由 7 个微服务组成：

- **C++ 服务**: GateServer, UMSServer, TaskServer, StatusServer, PushServer, PushServer2
- **Node.js 服务**: MailerServer

所有服务共享同一个 Redis 实例（`127.0.0.1:6379`）。当前 Redis 使用存在以下问题：

1. **零原子操作**: 没有 MULTI/EXEC、Lua 脚本、SET NX
2. **多步操作非原子**: LPUSH+LTRIM+EXPIRE、GET+DEL 等都是独立调用
3. **竞态条件**: Token 登录覆盖、Unread 计数器竞态、Online status 跨节点竞争

### 1.2 风险等级

| 并发问题 | 风险等级 | 影响 |
|---------|---------|------|
| Token 登录覆盖 | 🔴 严重 | 用户踢下线失败，安全隐患 |
| Unread 计数竞态 | 🔴 严重 | 未读数丢失或错误 |
| 消息缓存非原子 | 🟡 中等 | 崩溃时 key 无 TTL |
| Kick marker 竞态 | 🟡 中等 | 重复踢下线 |
| Online status 竞态 | 🟡 中等 | 用户在线状态错误 |
| 日志追加非原子 | 🟢 低 | 日志超出限制 |
| Mailer SET+EXPIRE | 🟢 低 | 验证码永不过期 |

---

## 二、解决方案概述

### 2.1 设计原则

**该用锁的地方用锁，该用 Lua 的地方用 Lua**：

- **分布式锁**: 用于需要业务逻辑判断的互斥操作（Token 登录、Unread 计数、Online status）
- **Lua 脚本**: 用于纯 Redis 命令打包（消息缓存、日志追加、Kick marker）

### 2.2 锁粒度选择

采用**中粒度**（per-uid）：
- 同一用户的操作需要互斥
- 不同用户之间完全不阻塞
- 平衡性能和安全性

### 2.3 总体架构

```
改造前:
  业务代码 → RedisManager::set/get/incr/setex（各自独立，无保护）

改造后:
  业务代码 → RedisManager::acquireLock/releaseLock（分布式锁）
           → RedisManager::evalScript（Lua 脚本）
           → RedisManager::pushMessageAtomic/markReadAtomic（专用封装）
```

---

## 三、RedisManager 新增 API

### 3.1 分布式锁 API

```cpp
// ============ RedisManager.h 新增方法 ============

// 加锁：SET key owner_id NX EX ttl
// 返回 true = 成功拿到锁
bool acquireLock(const std::string& lock_key, 
                 const std::string& owner_id, 
                 int ttl_seconds = 30);

// 释放锁：Lua 脚本原子比较+删除
// 返回 true = 成功释放（锁确实是自己的）
bool releaseLock(const std::string& lock_key, 
                 const std::string& owner_id);

// 带重试的加锁：指数退避
// max_retries = 最大重试次数，base_delay_ms = 初始等待毫秒
bool acquireLockWithRetry(const std::string& lock_key,
                          const std::string& owner_id,
                          int ttl_seconds = 30,
                          int max_retries = 3,
                          int base_delay_ms = 50);
```

#### 实现细节

**加锁 Lua 脚本**:
```lua
-- acquireLock.lua
local key = KEYS[1]
local owner = ARGV[1]
local ttl = ARGV[2]

if redis.call('SET', key, owner, 'NX', 'EX', ttl) then
    return 1
else
    return 0
end
```

**释放锁 Lua 脚本**:
```lua
-- releaseLock.lua
local key = KEYS[1]
local owner = ARGV[1]

if redis.call('GET', key) == owner then
    return redis.call('DEL', key)
else
    return 0
end
```

**指数退避逻辑**:
```cpp
bool acquireLockWithRetry(const std::string& lock_key,
                          const std::string& owner_id,
                          int ttl_seconds,
                          int max_retries,
                          int base_delay_ms) {
    for (int i = 0; i <= max_retries; ++i) {
        if (acquireLock(lock_key, owner_id, ttl_seconds)) {
            return true;
        }
        if (i < max_retries) {
            int delay = base_delay_ms * (1 << i);  // 50, 100, 200, 400...
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    return false;
}
```

### 3.2 Lua 脚本 API

```cpp
// 通用 Lua 执行
long long evalScript(const std::string& lua_script,
                     const std::vector<std::string>& keys,
                     const std::vector<std::string>& args);

// ============ 专用封装（隐藏 Lua 细节） ============

// 消息推送原子操作：LPUSH + LTRIM + EXPIRE + INCR
bool pushMessageAtomic(const std::string& uid_str, 
                       const std::string& msg_json,
                       int max_messages = 50,
                       int ttl_seconds = 604800);

// 标记已读：支持部分已读（DECR N 次）和全部已读（SET 0）
bool markReadAtomic(const std::string& uid_str, 
                    int decrement_count);  // 0 = 全部已读，>0 = 减N

// Kick marker 原子 get+delete
bool getAndDeleteKick(const std::string& uid_str, 
                      std::string& out_kick_value);

// 日志追加原子操作：LPUSH + LTRIM + EXPIRE
bool appendLogAtomic(const std::string& service_name,
                     const std::string& log_json,
                     int max_entries = 500,
                     int ttl_seconds = 604800);
```

#### Lua 脚本实现

**pushMessageAtomic.lua**:
```lua
local msg_key = KEYS[1]      -- msgs:{uid}
local counter_key = KEYS[2]  -- unread:{uid}
local msg = ARGV[1]
local max_msgs = tonumber(ARGV[2])
local ttl = tonumber(ARGV[3])

redis.call('LPUSH', msg_key, msg)
redis.call('LTRIM', msg_key, 0, max_msgs - 1)
redis.call('EXPIRE', msg_key, ttl)
redis.call('INCR', counter_key)

return 1
```

**markReadAtomic.lua**:
```lua
local counter_key = KEYS[1]  -- unread:{uid}
local count = tonumber(ARGV[1])
local ttl = tonumber(ARGV[2])

if count == 0 then
    -- 全部已读
    redis.call('SETEX', counter_key, ttl, 0)
else
    -- 部分已读：批量 DECR
    for i = 1, count do
        redis.call('DECR', counter_key)
    end
end

return 1
```

**getAndDeleteKick.lua**:
```lua
local key = KEYS[1]
local val = redis.call('GET', key)
if val then
    redis.call('DEL', key)
end
return val
```

**appendLogAtomic.lua**:
```lua
local key = KEYS[1]
local log_entry = ARGV[1]
local max_entries = tonumber(ARGV[2])
local ttl = tonumber(ARGV[3])

redis.call('LPUSH', key, log_entry)
redis.call('LTRIM', key, 0, max_entries - 1)
redis.call('EXPIRE', key, ttl)

return 1
```

---

## 四、各并发点修复方案

### 4.1 Token 登录竞态（StatusServer）

**文件**: `StatusServer/StatusServiceImpl.cpp` - `LoginUser()`

**问题**: 两个设备同时登录，GET→SETEX→条件SETEX 不是原子的。

**方案**: 用分布式锁保护整个登录流程。

**改造前**:
```cpp
std::string old_token;
std::string token_key = USER_TOKEN_PREFIX + std::to_string(req->uid());
bool has_old = RedisManager::getInstance().get(token_key, old_token);

resp->set_token(generate_unique_string());
if (!insertToken(req->uid(), resp->token())) {
    LOG_ERROR(...);
}

if (has_old && !old_token.empty() && old_token != resp->token()) {
    std::string kick_key = "kick:" + std::to_string(req->uid());
    RedisManager::getInstance().setex(kick_key, old_token, 60);
}
```

**改造后**:
```cpp
std::string uid_str = std::to_string(req->uid());
std::string lock_key = "lock:login:" + uid_str;
std::string owner = generate_uuid();

if (!RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
    LOG_ERROR("Failed to acquire login lock for uid: %s", uid_str.c_str());
    resp->set_error(1002);
    return;
}

// 临界区开始
std::string old_token;
std::string token_key = USER_TOKEN_PREFIX + uid_str;
bool has_old = RedisManager::getInstance().get(token_key, old_token);

resp->set_token(generate_unique_string());
if (!insertToken(req->uid(), resp->token())) {
    LOG_ERROR("Failed to insert token for uid: %s", uid_str.c_str());
    RedisManager::getInstance().releaseLock(lock_key, owner);
    resp->set_error(1002);
    return;
}

if (has_old && !old_token.empty() && old_token != resp->token()) {
    std::string kick_key = "kick:" + uid_str;
    RedisManager::getInstance().setex(kick_key, old_token, 60);
}
// 临界区结束

RedisManager::getInstance().releaseLock(lock_key, owner);
```

**锁参数**:
- Key: `lock:login:{uid}`
- TTL: 10 秒（足够覆盖整个登录流程）
- 重试: 3 次，指数退避 50ms

---

### 4.2 Unread 计数竞态（PushServer × 2 + GateServer）

**文件**:
- `PushServer/PushGrpcServiceImpl.cpp` - `pushMessageToUser()` 和 `MarkRead()`
- `PushServer2/PushGrpcServiceImpl.cpp` - 同上
- `GateServer/LogicSystem.cpp` - `handleMarkRead()`

**问题**:
- `INCR unread:1001`（新消息）和 `SETEX unread:1001 "0"`（全部已读）可以交错
- `DECR` 循环和 `SETEX "0"` 也可以交错

**方案**: 用分布式锁保护"标记已读"操作，同时将 `pushMessageToUser` 改用 Lua 脚本。

#### pushMessageToUser 改造

**改造前**:
```cpp
RedisManager::getInstance().lpush("msgs:" + uid_str, msg_json);
RedisManager::getInstance().ltrim("msgs:" + uid_str, 0, 49);
RedisManager::getInstance().expire("msgs:" + uid_str, 604800);
RedisManager::getInstance().incr("unread:" + uid_str);
```

**改造后**:
```cpp
std::string lock_key = "lock:unread:" + uid_str;
std::string owner = generate_uuid();

if (!RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
    LOG_ERROR("Failed to acquire unread lock for uid: %s", uid_str.c_str());
    return false;
}

// 临界区：调用 Lua 脚本（原子执行 4 条命令）
bool success = RedisManager::getInstance().pushMessageAtomic(uid_str, msg_json);

RedisManager::getInstance().releaseLock(lock_key, owner);
return success;
```

#### MarkRead 改造

**改造前**:
```cpp
if (ids.empty()) {
    RedisManager::getInstance().setex("unread:" + uid_str, "0", 604800);
} else {
    for (size_t i = 0; i < ids.size(); ++i) {
        RedisManager::getInstance().decr("unread:" + uid_str);
    }
}
```

**改造后**:
```cpp
std::string lock_key = "lock:unread:" + uid_str;
std::string owner = generate_uuid();

if (!RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
    LOG_ERROR("Failed to acquire unread lock for uid: %s", uid_str.c_str());
    return false;
}

// 临界区：调用 Lua 脚本
int decrement_count = ids.empty() ? 0 : ids.size();
bool success = RedisManager::getInstance().markReadAtomic(uid_str, decrement_count);

RedisManager::getInstance().releaseLock(lock_key, owner);
return success;
```

**锁参数**:
- Key: `lock:unread:{uid}`
- TTL: 10 秒
- 重试: 3 次，指数退避 50ms

---

### 4.3 Kick marker 原子 get+delete（PushServer × 2）

**文件**:
- `PushServer/LogicSystem.cpp` - WebSocket 连接处理
- `PushServer2/LogicSystem.cpp` - 同上

**问题**: GET 和 DEL 之间可能被其他操作插入。

**方案**: 封装为 Lua 脚本 `getAndDeleteKick()`，无需分布式锁。

**改造前**:
```cpp
if (RedisManager::getInstance().get(kick_key, kick_val)) {
    RedisManager::getInstance().del(kick_key);
    // ... kick old session ...
}
```

**改造后**:
```cpp
std::string kick_val;
if (RedisManager::getInstance().getAndDeleteKick(kick_key, kick_val)) {
    // ... kick old session ...
}
```

---

### 4.4 Online status 跨节点竞态（PushServer × 2）

**文件**:
- `PushServer/LogicSystem.cpp` - WebSocket 连接/断开
- `PushServer2/LogicSystem.cpp` - 同上

**问题**: PushServer1 的 `SETEX online:1001 "1" 300` 和 PushServer2 的 `DEL online:1001` 可以交错。

**方案**: 用分布式锁保护连接/断开操作。

#### WebSocket 连接改造

**改造前**:
```cpp
RedisManager::getInstance().setex("online:" + uid_str, "1", 300);
RedisManager::getInstance().setex("pushnode:" + uid_str, node_info, 86400);
```

**改造后**:
```cpp
std::string lock_key = "lock:online:" + uid_str;
std::string owner = generate_uuid();

if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
    // 临界区
    RedisManager::getInstance().setex("online:" + uid_str, "1", 300);
    RedisManager::getInstance().setex("pushnode:" + uid_str, node_info, 86400);
    RedisManager::getInstance().releaseLock(lock_key, owner);
}
```

#### WebSocket 断开改造

**改造前**:
```cpp
RedisManager::getInstance().del("online:" + uid_str);
```

**改造后**:
```cpp
std::string lock_key = "lock:online:" + uid_str;
std::string owner = generate_uuid();

if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
    // 临界区：检查 pushnode 是否是自己
    std::string current_node;
    if (RedisManager::getInstance().get("pushnode:" + uid_str, current_node)) {
        if (current_node == my_node_info) {
            RedisManager::getInstance().del("online:" + uid_str);
            RedisManager::getInstance().del("pushnode:" + uid_str);
        }
    }
    RedisManager::getInstance().releaseLock(lock_key, owner);
}
```

**锁参数**:
- Key: `lock:online:{uid}`
- TTL: 10 秒
- 重试: 3 次，指数退避 50ms

---

### 4.5 日志追加非原子（StatusServer）

**文件**: `StatusServer/StatusServiceImpl.cpp` - `ReportLog()`

**问题**: LPUSH + LTRIM + EXPIRE 三条命令可能被拆散。

**方案**: 封装为 Lua 脚本 `appendLogAtomic()`，无需分布式锁。

**改造前**:
```cpp
redis.lpush(redis_key, json);
redis.ltrim(redis_key, 0, 499);
redis.expire(redis_key, 604800);
```

**改造后**:
```cpp
RedisManager::getInstance().appendLogAtomic(service_name, json);
```

---

### 4.6 MailerServer 非原子 SET+EXPIRE

**文件**: `MailerServer/redis.js`

**问题**: `SET` 和 `EXPIRE` 分开调用。

**方案**: 改为 `SET key value EX ttl` 单条命令。

**改造前**:
```javascript
await RedisCli.set(key, value)
await RedisCli.expire(key, exptime)
```

**改造后**:
```javascript
await RedisCli.set(key, value, 'EX', exptime)
```

---

## 五、锁 Key 命名规范

| 锁 Key | 用途 | TTL | 粒度 |
|--------|------|-----|------|
| `lock:login:{uid}` | Token 登录互斥 | 10s | per-uid |
| `lock:unread:{uid}` | Unread 计数保护 | 10s | per-uid |
| `lock:online:{uid}` | Online status 保护 | 10s | per-uid |

**命名规则**: `lock:<业务>:<粒度标识>`

---

## 六、错误处理策略

### 6.1 加锁失败

```cpp
if (!acquireLockWithRetry(lock_key, owner, 10, 3, 50)) {
    // 方案1: 返回错误码，客户端重试
    resp->set_error(1002);
    return;
    
    // 方案2: 记录日志，降级处理
    LOG_WARN("Lock acquisition failed, proceeding without lock");
}
```

**推荐方案1**：返回错误码，让客户端重试。分布式锁的目的是保证数据一致性，加锁失败时降级处理可能导致数据不一致。

### 6.2 锁续期

**当前方案不实现 Watchdog 续期**，原因：
1. 所有临界区操作都在毫秒级完成
2. TTL 10 秒足够覆盖任何业务操作
3. 实现 Watchdog 需要额外的后台线程，增加复杂度

**未来扩展**：如果业务操作耗时超过 10 秒，可以考虑实现 Watchdog 机制。

---

## 七、性能影响评估

### 7.1 锁开销

每次加锁/释放锁需要 2 次 Redis 调用（SET NX + EVAL），延迟约 0.1-0.5ms。

**影响范围**：
- Token 登录：每次登录增加 ~1ms
- Unread 计数：每次消息推送/标记已读增加 ~1ms
- Online status：每次连接/断开增加 ~1ms

**结论**：对于用户可感知的操作（登录、消息推送），1ms 的额外延迟完全可以接受。

### 7.2 Lua 脚本开销

Lua 脚本将多条命令合并为一条，实际上**减少了** Redis 调用次数：

| 操作 | 改造前 | 改造后 |
|------|--------|--------|
| 消息推送 | 4 次调用 | 1 次 Lua |
| 标记已读 | N 次 DECR 或 1 次 SETEX | 1 次 Lua |
| 日志追加 | 3 次调用 | 1 次 Lua |

**结论**：Lua 脚本不仅解决了原子性问题，还提升了性能。

---

## 八、测试验证方案

### 8.1 单元测试

```bash
# 测试分布式锁
grpcurl -plaintext -d '{"uid":1}' localhost:50053 message.StatusService/LoginUser

# 测试消息推送原子性
grpcurl -plaintext -d '{"uid":1,"msg":"test"}' localhost:50054 message.PushService/PushMessage

# 测试标记已读
grpcurl -plaintext -d '{"uid":1,"ids":[]}' localhost:50054 message.PushService/MarkRead
```

### 8.2 并发测试

使用脚本模拟并发登录：
```bash
for i in {1..10}; do
    curl -X POST http://localhost:8080/status_login \
        -H "Content-Type: application/json" \
        -d "{\"uid\":1,\"token\":\"test$i\"}" &
done
wait
```

验证：
1. Token 只有一个生效
2. Kick marker 正确设置
3. 没有数据竞争

### 8.3 崩溃恢复测试

1. 在 Lua 脚本执行中途 kill 进程
2. 重启后检查 key 的 TTL 是否正确设置
3. 验证数据一致性

---

## 九、实施步骤

### Phase 1: RedisManager 基础设施（所有服务）
1. 在 `RedisManager.h/.cpp` 中添加分布式锁 API
2. 在 `RedisManager.h/.cpp` 中添加 Lua 脚本 API
3. 添加专用封装函数
4. 编译验证

### Phase 2: Token 登录修复（StatusServer）
1. 修改 `StatusServiceImpl::LoginUser()`
2. 测试并发登录场景

### Phase 3: Unread 计数修复（PushServer × 2 + GateServer）
1. 修改 `PushGrpcServiceImpl::pushMessageToUser()`
2. 修改 `PushGrpcServiceImpl::MarkRead()`
3. 修改 `GateServer::LogicSystem::handleMarkRead()`
4. 测试消息推送和标记已读

### Phase 4: 其他修复
1. Kick marker 改用 Lua（PushServer × 2）
2. Online status 加锁（PushServer × 2）
3. 日志追加改用 Lua（StatusServer）
4. MailerServer SET+EXPIRE 合并

### Phase 5: 集成测试
1. 全链路测试
2. 并发压力测试
3. 崩溃恢复测试

---

## 十、参考资料

- [Redis 分布式锁](https://redis.io/docs/manual/patterns/distributed-locks/)
- [Redis Lua 脚本](https://redis.io/docs/manual/programmability/eval-intro/)
- [Redlock 算法](https://redis.io/docs/manual/patterns/distributed-locks/#the-redlock-algorithm)

---

## 附录 A：完整 Lua 脚本清单

### A.1 acquireLock.lua
```lua
local key = KEYS[1]
local owner = ARGV[1]
local ttl = ARGV[2]

if redis.call('SET', key, owner, 'NX', 'EX', ttl) then
    return 1
else
    return 0
end
```

### A.2 releaseLock.lua
```lua
local key = KEYS[1]
local owner = ARGV[1]

if redis.call('GET', key) == owner then
    return redis.call('DEL', key)
else
    return 0
end
```

### A.3 pushMessageAtomic.lua
```lua
local msg_key = KEYS[1]      -- msgs:{uid}
local counter_key = KEYS[2]  -- unread:{uid}
local msg = ARGV[1]
local max_msgs = tonumber(ARGV[2])
local ttl = tonumber(ARGV[3])

redis.call('LPUSH', msg_key, msg)
redis.call('LTRIM', msg_key, 0, max_msgs - 1)
redis.call('EXPIRE', msg_key, ttl)
redis.call('INCR', counter_key)

return 1
```

### A.4 markReadAtomic.lua
```lua
local counter_key = KEYS[1]  -- unread:{uid}
local count = tonumber(ARGV[1])
local ttl = tonumber(ARGV[2])

if count == 0 then
    -- 全部已读
    redis.call('SETEX', counter_key, ttl, 0)
else
    -- 部分已读：批量 DECR
    for i = 1, count do
        redis.call('DECR', counter_key)
    end
end

return 1
```

### A.5 getAndDeleteKick.lua
```lua
local key = KEYS[1]
local val = redis.call('GET', key)
if val then
    redis.call('DEL', key)
end
return val
```

### A.6 appendLogAtomic.lua
```lua
local key = KEYS[1]
local log_entry = ARGV[1]
local max_entries = tonumber(ARGV[2])
local ttl = tonumber(ARGV[3])

redis.call('LPUSH', key, log_entry)
redis.call('LTRIM', key, 0, max_entries - 1)
redis.call('EXPIRE', key, ttl)

return 1
```

---

## 附录 B：Redis Key 完整清单

| Key Pattern | 类型 | TTL | 用途 | 设置位置 |
|-------------|------|-----|------|---------|
| `utoken_{uid}` | STRING | 86400s | 认证 Token | StatusServiceImpl.cpp |
| `code_{email}` | STRING | 180s | 邮箱验证码 | UMSGrpcServiceImpl.cpp |
| `kick:{uid}` | STRING | 60s | 踢下线标记 | StatusServiceImpl.cpp |
| `pushnode:{uid}` | STRING | 86400s | PushServer 节点映射 | StatusServiceImpl.cpp |
| `online:{uid}` | STRING | 300s | 在线状态 | LogicSystem.cpp |
| `unread:{uid}` | STRING | 604800s | 未读消息计数 | PushGrpcServiceImpl.cpp |
| `msgs:{uid}` | LIST | 604800s | 消息缓存（最近 50 条） | PushGrpcServiceImpl.cpp |
| `logs:{ServiceName}` | LIST | 604800s | 服务日志（最近 500 条） | StatusServiceImpl.cpp |
| `lock:login:{uid}` | STRING | 10s | 登录互斥锁 | 新增 |
| `lock:unread:{uid}` | STRING | 10s | 未读计数保护锁 | 新增 |
| `lock:online:{uid}` | STRING | 10s | 在线状态保护锁 | 新增 |

---

**文档版本**: v1.0  
**最后更新**: 2026-05-31
