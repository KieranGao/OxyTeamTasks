# Redis Distributed Lock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix 7 Redis concurrency issues across all microservices by adding distributed locks and Lua scripts.

**Architecture:** Add `acquireLock`/`releaseLock` (distributed lock) and `evalScript`/specialized wrappers (Lua scripts) to RedisManager. Each server's RedisManager gets the same new methods. Then fix each concurrent access point using the appropriate mechanism.

**Tech Stack:** C++ (hiredis), Node.js (ioredis), Lua scripts (Redis EVAL)

---

## File Structure

### Files to Modify (RedisManager — 6 copies, one per C++ server)

| File | Responsibility |
|------|---------------|
| `GateServer/RedisManager.h` / `.cpp` | Add distributed lock + Lua script methods |
| `PushServer/RedisManager.h` / `.cpp` | Same |
| `PushServer2/RedisManager.h` / `.cpp` | Same |
| `StatusServer/RedisManager.h` / `.cpp` | Same |
| `TaskServer/RedisManager.h` / `.cpp` | Same |
| `UMSServer/RedisManager.h` / `.cpp` | Same |

### Files to Modify (Concurrency Fixes)

| File | Fix |
|------|-----|
| `StatusServer/StatusServiceImpl.cpp` | Token login race + Log append atomicity |
| `PushServer/PushGrpcServiceImpl.cpp` | Unread counter + message cache atomicity |
| `PushServer2/PushGrpcServiceImpl.cpp` | Same as PushServer |
| `GateServer/LogicSystem.cpp` | Unread counter in markRead |
| `PushServer/LogicSystem.cpp` | Kick marker + online status |
| `PushServer2/LogicSystem.cpp` | Same as PushServer |
| `PushServer/MainServer.cpp` | Online status cleanup on disconnect |
| `PushServer2/MainServer.cpp` | Same as PushServer |
| `MailerServer/redis.js` | SET+EXPIRE atomicity |

---

## Task 1: Add Distributed Lock Methods to RedisManager (GateServer)

**Files:**
- Modify: `GateServer/RedisManager.h`
- Modify: `GateServer/RedisManager.cpp`

- [ ] **Step 1: Add new method declarations to RedisManager.h**

Add these declarations after the existing `decr` method (line 25):

```cpp
    // ============ Distributed Lock ============
    // Acquire lock: SET key owner_id NX EX ttl
    bool acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds = 30);
    // Release lock: Lua script atomic compare+delete
    bool releaseLock(const std::string& lock_key, const std::string& owner_id);
    // Acquire lock with exponential backoff retry
    bool acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                              int ttl_seconds = 30, int max_retries = 3, int base_delay_ms = 50);

    // ============ Lua Script Execution ============
    // Generic EVAL: returns integer result
    long long evalScript(const std::string& lua_script,
                         const std::vector<std::string>& keys,
                         const std::vector<std::string>& args);

    // Atomic message push: LPUSH + LTRIM + EXPIRE + INCR
    bool pushMessageAtomic(const std::string& uid_str, const std::string& msg_json,
                           int max_messages = 50, int ttl_seconds = 604800);
    // Atomic mark-read: SETEX(0) for all, or DECR N times
    bool markReadAtomic(const std::string& uid_str, int decrement_count, int ttl_seconds = 604800);
    // Atomic kick marker: GET + DEL
    bool getAndDeleteKick(const std::string& uid_str, std::string& out_kick_value);
    // Atomic log append: LPUSH + LTRIM + EXPIRE
    bool appendLogAtomic(const std::string& service_name, const std::string& log_json,
                         int max_entries = 500, int ttl_seconds = 604800);
```

- [ ] **Step 2: Add include for `<thread>` and `<chrono>` to RedisManager.cpp**

Add at the top of `GateServer/RedisManager.cpp` after existing includes:

```cpp
#include <thread>
#include <chrono>
```

- [ ] **Step 3: Implement acquireLock in RedisManager.cpp**

Add after the `decr` method implementation:

```cpp
bool RedisManager::acquireLock(const std::string& lock_key, const std::string& owner_id, int ttl_seconds) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "SET %s %s NX EX %d", lock_key.c_str(), owner_id.c_str(), ttl_seconds));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute SET NX EX for lock: {}", lock_key);
        return false;
    }
    bool ok = (reply->type == REDIS_REPLY_STATUS && strcmp(reply->str, "OK") == 0);
    freeReplyObject(reply);
    if (ok) {
        LOG_DEBUG("Lock acquired: {} owner={}", lock_key, owner_id);
    }
    return ok;
}
```

- [ ] **Step 4: Implement releaseLock in RedisManager.cpp**

```cpp
bool RedisManager::releaseLock(const std::string& lock_key, const std::string& owner_id) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;
    // Lua: if GET(key) == owner then DEL(key) else return 0
    const char* lua = "if redis.call('GET',KEYS[1])==ARGV[1] then return redis.call('DEL',KEYS[1]) else return 0 end";
    redisReply* reply = static_cast<redisReply*>(redisCommand(connect.get(),
        "EVAL %s 1 %s %s", lua, lock_key.c_str(), owner_id.c_str()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute EVAL for releaseLock: {}", lock_key);
        return false;
    }
    bool released = (reply->type == REDIS_REPLY_INTEGER && reply->integer == 1);
    freeReplyObject(reply);
    if (released) {
        LOG_DEBUG("Lock released: {} owner={}", lock_key, owner_id);
    }
    return released;
}
```

- [ ] **Step 5: Implement acquireLockWithRetry in RedisManager.cpp**

```cpp
bool RedisManager::acquireLockWithRetry(const std::string& lock_key, const std::string& owner_id,
                                         int ttl_seconds, int max_retries, int base_delay_ms) {
    for (int i = 0; i <= max_retries; ++i) {
        if (acquireLock(lock_key, owner_id, ttl_seconds)) {
            return true;
        }
        if (i < max_retries) {
            int delay = base_delay_ms * (1 << i);  // 50, 100, 200, 400...
            std::this_thread::sleep_for(std::chrono::milliseconds(delay));
        }
    }
    LOG_WARN("Lock acquisition failed after {} retries: {}", max_retries, lock_key);
    return false;
}
```

- [ ] **Step 6: Implement evalScript in RedisManager.cpp**

```cpp
long long RedisManager::evalScript(const std::string& lua_script,
                                    const std::vector<std::string>& keys,
                                    const std::vector<std::string>& args) {
    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return -1;

    // Build argv array for redisCommandArgv
    // Command: EVAL <script> <numkeys> [keys...] [args...]
    std::vector<std::string> parts;
    parts.push_back("EVAL");
    parts.push_back(lua_script);
    parts.push_back(std::to_string(keys.size()));
    for (const auto& k : keys) parts.push_back(k);
    for (const auto& a : args) parts.push_back(a);

    std::vector<const char*> argv(parts.size());
    std::vector<size_t> argvlen(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        argv[i] = parts[i].c_str();
        argvlen[i] = parts[i].size();
    }

    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        connect.get(), static_cast<int>(parts.size()), argv.data(), argvlen.data()));
    conn_pool_->returnConnection(std::move(connect));
    if (reply == nullptr) {
        LOG_ERROR("Failed to execute EVAL script");
        return -1;
    }
    long long result = 0;
    if (reply->type == REDIS_REPLY_INTEGER) {
        result = reply->integer;
    } else if (reply->type == REDIS_REPLY_STRING) {
        try { result = std::stoll(reply->str); } catch (...) { result = -1; }
    } else if (reply->type == REDIS_REPLY_NIL) {
        result = 0;
    }
    freeReplyObject(reply);
    return result;
}
```

- [ ] **Step 7: Implement pushMessageAtomic in RedisManager.cpp**

```cpp
bool RedisManager::pushMessageAtomic(const std::string& uid_str, const std::string& msg_json,
                                      int max_messages, int ttl_seconds) {
    std::string lua =
        "local msg_key=KEYS[1] "
        "local counter_key=KEYS[2] "
        "local msg=ARGV[1] "
        "local max_msgs=tonumber(ARGV[2]) "
        "local ttl=tonumber(ARGV[3]) "
        "redis.call('LPUSH',msg_key,msg) "
        "redis.call('LTRIM',msg_key,0,max_msgs-1) "
        "redis.call('EXPIRE',msg_key,ttl) "
        "redis.call('INCR',counter_key) "
        "return 1";
    std::vector<std::string> keys = {"msgs:" + uid_str, "unread:" + uid_str};
    std::vector<std::string> args = {msg_json, std::to_string(max_messages), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("pushMessageAtomic failed for uid={}", uid_str);
        return false;
    }
    LOG_DEBUG("pushMessageAtomic success for uid={}", uid_str);
    return true;
}
```

- [ ] **Step 8: Implement markReadAtomic in RedisManager.cpp**

```cpp
bool RedisManager::markReadAtomic(const std::string& uid_str, int decrement_count, int ttl_seconds) {
    std::string lua;
    if (decrement_count == 0) {
        // Mark all read: SETEX to 0
        lua = "redis.call('SETEX',KEYS[1],ARGV[2],0) return 1";
    } else {
        // Partial read: DECR N times
        lua = "local n=tonumber(ARGV[1]) for i=1,n do redis.call('DECR',KEYS[1]) end return 1";
    }
    std::vector<std::string> keys = {"unread:" + uid_str};
    std::vector<std::string> args = {std::to_string(decrement_count), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("markReadAtomic failed for uid={}", uid_str);
        return false;
    }
    LOG_DEBUG("markReadAtomic success for uid={} count={}", uid_str, decrement_count);
    return true;
}
```

- [ ] **Step 9: Implement getAndDeleteKick in RedisManager.cpp**

```cpp
bool RedisManager::getAndDeleteKick(const std::string& uid_str, std::string& out_kick_value) {
    std::string lua =
        "local val=redis.call('GET',KEYS[1]) "
        "if val then redis.call('DEL',KEYS[1]) end "
        "return val";
    std::vector<std::string> keys = {"kick:" + uid_str};
    std::vector<std::string> args = {};

    auto connect = conn_pool_->getConnection();
    if (connect == nullptr) return false;

    // Build EVAL command
    std::vector<std::string> parts = {"EVAL", lua, "1", "kick:" + uid_str};
    std::vector<const char*> argv(parts.size());
    std::vector<size_t> argvlen(parts.size());
    for (size_t i = 0; i < parts.size(); ++i) {
        argv[i] = parts[i].c_str();
        argvlen[i] = parts[i].size();
    }

    redisReply* reply = static_cast<redisReply*>(redisCommandArgv(
        connect.get(), static_cast<int>(parts.size()), argv.data(), argvlen.data()));
    conn_pool_->returnConnection(std::move(connect));

    if (reply == nullptr) {
        LOG_ERROR("getAndDeleteKick EVAL failed for uid={}", uid_str);
        return false;
    }
    if (reply->type == REDIS_REPLY_STRING) {
        out_kick_value = std::string(reply->str, reply->len);
        freeReplyObject(reply);
        LOG_DEBUG("getAndDeleteKick: found kick marker for uid={}", uid_str);
        return true;
    }
    freeReplyObject(reply);
    return false;
}
```

- [ ] **Step 10: Implement appendLogAtomic in RedisManager.cpp**

```cpp
bool RedisManager::appendLogAtomic(const std::string& service_name, const std::string& log_json,
                                    int max_entries, int ttl_seconds) {
    std::string lua =
        "local key=KEYS[1] "
        "local entry=ARGV[1] "
        "local max_e=tonumber(ARGV[2]) "
        "local ttl=tonumber(ARGV[3]) "
        "redis.call('LPUSH',key,entry) "
        "redis.call('LTRIM',key,0,max_e-1) "
        "redis.call('EXPIRE',key,ttl) "
        "return 1";
    std::vector<std::string> keys = {"logs:" + service_name};
    std::vector<std::string> args = {log_json, std::to_string(max_entries), std::to_string(ttl_seconds)};
    long long result = evalScript(lua, keys, args);
    if (result < 0) {
        LOG_ERROR("appendLogAtomic failed for service={}", service_name);
        return false;
    }
    return true;
}
```

- [ ] **Step 11: Build and verify compilation**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/GateServer && cmake --build build
```

Expected: Build succeeds with no errors.

- [ ] **Step 12: Commit**

```bash
git add GateServer/RedisManager.h GateServer/RedisManager.cpp
git commit -m "feat(GateServer): add distributed lock and Lua script methods to RedisManager"
```

---

## Task 2: Add Same Methods to PushServer RedisManager

**Files:**
- Modify: `PushServer/RedisManager.h`
- Modify: `PushServer/RedisManager.cpp`

- [ ] **Step 1: Copy new declarations to PushServer/RedisManager.h**

Add after the existing `decr` method (line 26). The declarations are identical to Task 1 Step 1.

- [ ] **Step 2: Copy implementations to PushServer/RedisManager.cpp**

Add the same includes (`<thread>`, `<chrono>`) and all method implementations from Task 1 Steps 3-10.

- [ ] **Step 3: Build**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add PushServer/RedisManager.h PushServer/RedisManager.cpp
git commit -m "feat(PushServer): add distributed lock and Lua script methods to RedisManager"
```

---

## Task 3: Add Same Methods to PushServer2 RedisManager

**Files:**
- Modify: `PushServer2/RedisManager.h`
- Modify: `PushServer2/RedisManager.cpp`

- [ ] **Step 1: Copy new declarations to PushServer2/RedisManager.h**

Identical to Task 2.

- [ ] **Step 2: Copy implementations to PushServer2/RedisManager.cpp**

Identical to Task 2.

- [ ] **Step 3: Build**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer2 && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add PushServer2/RedisManager.h PushServer2/RedisManager.cpp
git commit -m "feat(PushServer2): add distributed lock and Lua script methods to RedisManager"
```

---

## Task 4: Add Same Methods to StatusServer RedisManager

**Files:**
- Modify: `StatusServer/RedisManager.h`
- Modify: `StatusServer/RedisManager.cpp`

- [ ] **Step 1: Copy new declarations to StatusServer/RedisManager.h**

Identical to Task 2.

- [ ] **Step 2: Copy implementations to StatusServer/RedisManager.cpp**

Identical to Task 2.

- [ ] **Step 3: Build**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/StatusServer && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add StatusServer/RedisManager.h StatusServer/RedisManager.cpp
git commit -m "feat(StatusServer): add distributed lock and Lua script methods to RedisManager"
```

---

## Task 5: Add Same Methods to TaskServer and UMSServer RedisManager

**Files:**
- Modify: `TaskServer/RedisManager.h` / `.cpp`
- Modify: `UMSServer/RedisManager.h` / `.cpp`

- [ ] **Step 1: Copy to TaskServer**

Same as Task 2.

- [ ] **Step 2: Copy to UMSServer**

Same as Task 2.

- [ ] **Step 3: Build both**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/TaskServer && cmake --build build
cd /home/oxythecrack/Desktop/OxyTasks/UMSServer && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add TaskServer/RedisManager.h TaskServer/RedisManager.cpp UMSServer/RedisManager.h UMSServer/RedisManager.cpp
git commit -m "feat(TaskServer,UMSServer): add distributed lock and Lua script methods to RedisManager"
```

---

## Task 6: Fix Token Login Race Condition (StatusServer)

**Files:**
- Modify: `StatusServer/StatusServiceImpl.cpp:87-116`

- [ ] **Step 1: Wrap AllocatePushServer login logic with distributed lock**

Replace lines 94-109 of `StatusServer/StatusServiceImpl.cpp`:

```cpp
    // Check if old token exists before overwriting — if so, mark for kicking old session
    std::string uid_str = std::to_string(req->uid());
    std::string lock_key = "lock:login:" + uid_str;
    std::string owner = generate_unique_string();

    if (!RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
        LOG_ERROR("[StatusServer] AllocatePushServer: Failed to acquire login lock for uid={}", req->uid());
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }

    // Critical section: token read-write-kick
    std::string old_token;
    std::string token_key = USER_TOKEN_PREFIX + uid_str;
    bool has_old = RedisManager::getInstance().get(token_key, old_token);

    resp->set_token(generate_unique_string());
    if (!insertToken(req->uid(), resp->token())) {
        LOG_ERROR("[StatusServer] AllocatePushServer: Failed to persist token for uid={}, Redis may be down", req->uid());
        RedisManager::getInstance().releaseLock(lock_key, owner);
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }

    // If old token existed and was different, set kick marker for PushServer to close old session
    if (has_old && !old_token.empty() && old_token != resp->token()) {
        std::string kick_key = "kick:" + uid_str;
        RedisManager::getInstance().setex(kick_key, old_token, 60);
        LOG_INFO("[StatusServer] Set kick marker for uid={}", req->uid());
    }

    RedisManager::getInstance().releaseLock(lock_key, owner);
```

- [ ] **Step 2: Build StatusServer**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/StatusServer && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add StatusServer/StatusServiceImpl.cpp
git commit -m "fix(StatusServer): wrap token login with distributed lock to prevent race condition"
```

---

## Task 7: Fix Log Append Atomicity (StatusServer)

**Files:**
- Modify: `StatusServer/StatusServiceImpl.cpp:175-188`

- [ ] **Step 1: Replace ReportLog's Redis calls with appendLogAtomic**

Replace lines 184-187:

```cpp
        auto& redis = RedisManager::getInstance();
        redis.appendLogAtomic(entry.service(), json);
```

- [ ] **Step 2: Build StatusServer**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/StatusServer && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add StatusServer/StatusServiceImpl.cpp
git commit -m "fix(StatusServer): use atomic Lua script for log append"
```

---

## Task 8: Fix Message Push + Unread Counter (PushServer)

**Files:**
- Modify: `PushServer/PushGrpcServiceImpl.cpp:50-55` (pushMessageToUser)
- Modify: `PushServer/PushGrpcServiceImpl.cpp:198-221` (MarkRead)

- [ ] **Step 1: Add boost uuid includes to PushGrpcServiceImpl.cpp**

Add after existing includes (line 9):

```cpp
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
```

Add a local helper function after the `parseNodeValue` function (after line 28):

```cpp
static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
```

- [ ] **Step 2: Replace pushMessageToUser's Redis calls with lock + pushMessageAtomic**

Replace lines 52-55:

```cpp
    // Atomic: LPUSH + LTRIM + EXPIRE + INCR, protected by distributed lock
    std::string lock_key = "lock:unread:" + uid_str;
    std::string owner = generate_lock_owner();
    if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
        RedisManager::getInstance().pushMessageAtomic(uid_str, msg_json);
        RedisManager::getInstance().releaseLock(lock_key, owner);
    } else {
        LOG_ERROR("[Push] Failed to acquire unread lock for pushMessage uid={}", uid_str);
    }
```

- [ ] **Step 3: Replace MarkRead's Redis calls with lock + markReadAtomic**

Replace lines 210-217:

```cpp
    // Update Redis unread count — protected by distributed lock
    std::string uid_str = std::to_string(uid);
    std::string lock_key = "lock:unread:" + uid_str;
    std::string owner = generate_lock_owner();
    if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
        int decrement_count = ids.empty() ? 0 : static_cast<int>(ids.size());
        RedisManager::getInstance().markReadAtomic(uid_str, decrement_count);
        RedisManager::getInstance().releaseLock(lock_key, owner);
    } else {
        LOG_ERROR("[Push] Failed to acquire unread lock for MarkRead uid={}", uid);
    }
```

- [ ] **Step 4: Build PushServer**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer && cmake --build build
```

- [ ] **Step 5: Commit**

```bash
git add PushServer/PushGrpcServiceImpl.cpp
git commit -m "fix(PushServer): use distributed lock + Lua for message push and mark-read"
```

---

## Task 9: Fix Message Push + Unread Counter (PushServer2)

**Files:**
- Modify: `PushServer2/PushGrpcServiceImpl.cpp`

- [ ] **Step 1: Apply identical changes as Task 8**

The file is a copy of PushServer's. Apply the same changes:
1. Add boost uuid includes
2. Add `generate_lock_owner()` helper
3. Replace pushMessageToUser's Redis calls (lines 52-55)
4. Replace MarkRead's Redis calls (lines 210-217)

- [ ] **Step 2: Build PushServer2**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer2 && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add PushServer2/PushGrpcServiceImpl.cpp
git commit -m "fix(PushServer2): use distributed lock + Lua for message push and mark-read"
```

---

## Task 10: Fix Unread Counter in GateServer markRead

**Files:**
- Modify: `GateServer/LogicSystem.cpp:673-680`

- [ ] **Step 1: Add boost uuid includes to LogicSystem.cpp**

Add after existing includes (line 8):

```cpp
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
```

Add a local helper function after the includes:

```cpp
static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
```

- [ ] **Step 2: Replace the markRead Redis calls with lock + markReadAtomic**

Replace lines 673-680:

```cpp
        MySQLManager::getInstance().markMessagesRead(uid, ids);
        // Atomic mark-read with distributed lock protection
        std::string uid_str = std::to_string(uid);
        std::string lock_key = "lock:unread:" + uid_str;
        std::string owner = generate_lock_owner();
        if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
            int decrement_count = ids.empty() ? 0 : static_cast<int>(ids.size());
            RedisManager::getInstance().markReadAtomic(uid_str, decrement_count);
            RedisManager::getInstance().releaseLock(lock_key, owner);
        } else {
            LOG_ERROR("[Gate] Failed to acquire unread lock for markRead uid={}", uid);
        }
```

- [ ] **Step 3: Build GateServer**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/GateServer && cmake --build build
```

- [ ] **Step 4: Commit**

```bash
git add GateServer/LogicSystem.cpp
git commit -m "fix(GateServer): use distributed lock + Lua for markRead unread counter"
```

---

## Task 11: Fix Kick Marker + Online Status (PushServer)

**Files:**
- Modify: `PushServer/LogicSystem.cpp:135-155` (kick marker + online status)
- Modify: `PushServer/MainServer.cpp:62-65` (disconnect cleanup)

- [ ] **Step 1: Add boost uuid includes to LogicSystem.cpp**

Add after existing includes (line 7):

```cpp
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
```

Add a local helper function:

```cpp
static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
```

- [ ] **Step 2: Replace kick marker GET+DEL with getAndDeleteKick**

Replace lines 136-150:

```cpp
    // Atomic kick marker check: GET + DEL in one Lua call
    std::string uid_str = std::to_string(uid);
    std::string kick_key = "kick:" + uid_str;
    std::string kick_val;
    if (RedisManager::getInstance().getAndDeleteKick(uid_str, kick_val)) {
        auto oldSession = session->getServer()->getSessionByUid(uid);
        if (oldSession) {
            LOG_INFO("[PushServer] Kicking old session for uid={}", uid);
            Json::Value kickMsg;
            kickMsg["type"] = WS_MSG_KICKED;
            kickMsg["reason"] = "other_login";
            oldSession->send(kickMsg.toStyledString());
            oldSession->close();
            session->getServer()->removeUidSession(uid);
        }
    }
```

- [ ] **Step 3: Wrap online status set with distributed lock**

Replace line 155:

```cpp
    // Set online status with distributed lock protection
    std::string online_lock = "lock:online:" + uid_str;
    std::string lock_owner = generate_lock_owner();
    if (RedisManager::getInstance().acquireLockWithRetry(online_lock, lock_owner, 10)) {
        RedisManager::getInstance().setex("online:" + uid_str, "1", 300);
        RedisManager::getInstance().releaseLock(online_lock, lock_owner);
    }
```

- [ ] **Step 4: Add boost uuid includes to MainServer.cpp**

Add after existing includes (line 6):

```cpp
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
```

Add a local helper:

```cpp
static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}
```

- [ ] **Step 5: Wrap disconnect cleanup with distributed lock**

Replace lines 62-65 in MainServer.cpp:

```cpp
    if (uid > 0) {
        removeUidSession(uid);
        // Delete online status with distributed lock protection
        std::string uid_str = std::to_string(uid);
        std::string lock_key = "lock:online:" + uid_str;
        std::string owner = generate_lock_owner();
        if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
            // Only delete if this node still owns the online status
            std::string current_node;
            if (RedisManager::getInstance().get("pushnode:" + uid_str, current_node)) {
                // Check if this is our node (by comparing with our server name)
                if (current_node.find(server_name_) != std::string::npos) {
                    RedisManager::getInstance().del("online:" + uid_str);
                }
            }
            RedisManager::getInstance().releaseLock(lock_key, owner);
        }
    }
```

- [ ] **Step 6: Build PushServer**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer && cmake --build build
```

- [ ] **Step 7: Commit**

```bash
git add PushServer/LogicSystem.cpp PushServer/MainServer.cpp
git commit -m "fix(PushServer): use Lua for kick marker, lock for online status"
```

---

## Task 12: Fix Kick Marker + Online Status (PushServer2)

**Files:**
- Modify: `PushServer2/LogicSystem.cpp`
- Modify: `PushServer2/MainServer.cpp`

- [ ] **Step 1: Apply identical changes as Task 11**

The files are copies of PushServer's. Apply the same changes:
1. Add boost uuid includes to both files
2. Add `generate_lock_owner()` helper to both files
3. Replace kick marker logic in LogicSystem.cpp
4. Wrap online status set with lock in LogicSystem.cpp
5. Wrap disconnect cleanup with lock in MainServer.cpp

- [ ] **Step 2: Build PushServer2**

```bash
cd /home/oxythecrack/Desktop/OxyTasks/PushServer2 && cmake --build build
```

- [ ] **Step 3: Commit**

```bash
git add PushServer2/LogicSystem.cpp PushServer2/MainServer.cpp
git commit -m "fix(PushServer2): use Lua for kick marker, lock for online status"
```

---

## Task 13: Fix MailerServer SET+EXPIRE

**Files:**
- Modify: `MailerServer/redis.js:63-74`

- [ ] **Step 1: Replace SET + EXPIRE with atomic SET EX**

Replace the `SetRedisExpire` function:

```javascript
async function SetRedisExpire(key, value, exptime){
    try{
        // Atomic: SET key value EX exptime (single command)
        await RedisCli.set(key, value, 'EX', exptime);
        return true;
    }catch(error){
        console.log('SetRedisExpire error is', error);
        return false;
    }
}
```

- [ ] **Step 2: Commit**

```bash
git add MailerServer/redis.js
git commit -m "fix(MailerServer): use atomic SET EX instead of separate SET + EXPIRE"
```

---

## Task 14: Build All and Verify

- [ ] **Step 1: Build all C++ servers**

```bash
cd /home/oxythecrack/Desktop/OxyTasks && ./build_all.sh
```

Expected: All 6 servers build successfully.

- [ ] **Step 2: Commit any remaining changes**

```bash
git add -A
git commit -m "chore: all servers build successfully with distributed lock changes"
```

---

## Summary of Changes

| Task | File | Change Type | Lock/Lua |
|------|------|-------------|----------|
| 1-5 | RedisManager.h/.cpp × 6 | New methods | Both |
| 6 | StatusServiceImpl.cpp | Token login | Lock |
| 7 | StatusServiceImpl.cpp | Log append | Lua |
| 8-9 | PushGrpcServiceImpl.cpp × 2 | Message push + markRead | Lock + Lua |
| 10 | LogicSystem.cpp (Gate) | MarkRead | Lock + Lua |
| 11-12 | LogicSystem.cpp + MainServer.cpp × 2 | Kick + online status | Lua + Lock |
| 13 | redis.js | SET+EXPIRE | Atomic cmd |

**Total files modified:** 15 files across 7 services
