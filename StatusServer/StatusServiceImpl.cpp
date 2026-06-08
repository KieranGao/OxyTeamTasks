#include "StatusServiceImpl.h"
#include "ConfigManager.h"
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"
#include "RedisManager.h"
#include <string>
#include <vector>
#include <json/json.h>
#include "Logger.h"

std::string generate_unique_string()
{
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    std::string unique_string = boost::uuids::to_string(uuid);
    return unique_string;
}

StatusServiceImpl::StatusServiceImpl()
{
    auto &g_config = ConfigManager::getInstance();
    auto server_list = g_config["PushServers"]["name"];
    allocate_method_ = g_config["AllocateMethods"]["methods"];
    std::vector<std::string> names;
    std::stringstream ss(server_list);
    std::string name;
    while(std::getline(ss,name,',')) names.push_back(name);
    server_cnt_ = names.size();
    if(server_cnt_ == 0) {
        LOG_ERROR("[StatusServer] FATAL: No PushServers configured in [PushServers] section!");
        std::abort();
    }
    if(allocate_method_ == "SegmentTree")
        SegTree_ = std::make_unique<SegmentTree>(std::vector<int>(server_cnt_ + 1));
    else {
        allocate_method_ = "Brute";
        server_conns_.resize(server_cnt_ + 1, 0);
    }
    int idx = 1;
    for(auto &name : names)
    {
        if(g_config[name]["name"].empty()) continue;
        PushServer server;
        server.host = g_config[name]["host"];
        server.port = g_config[name]["port"];
        server.grpc_port = g_config[name]["gRPC_port"];
        server.name = g_config[name]["name"];
        server.id = idx;
        // 以 host:port 作为唯一标识
        std::string key = server.host + ":" + server.port;
        servers_[key] = server;
        servers_idx_[idx++] = server;
    }
}

PushServer& StatusServiceImpl::selectPushServer() {
    std::lock_guard<std::mutex> lock(server_mtx_);
    assert(!servers_.empty());
    if(allocate_method_ == "Brute") {
        int minCon = INT_MAX;
        int minIdx = 1;
        for(int i=1;i<=server_cnt_;i++) {
            if(minCon >= server_conns_[i]) {
                minCon = server_conns_[i];
                minIdx = i;
            }
        }
        return servers_idx_[minIdx];
    } else {
        int minIdx = SegTree_->queryMinidx(1,server_cnt_);
        return servers_idx_[minIdx];
    }
}

// Caller must hold server_mtx_
void StatusServiceImpl::returnServer(PushServer& cs) {
    if(allocate_method_ == "Brute") {
        if(cs.id > 0 and cs.id <= server_cnt_ and server_conns_[cs.id] > 0) server_conns_[cs.id]--;
    } else {
        int cur = SegTree_->getVal(cs.id);
        if(cur > 0) SegTree_->updateVal(cs.id, cur - 1);
    }
}

Status StatusServiceImpl::AllocatePushServer(ServerContext* context, const AllocateReq* req, AllocateRsp* resp)
{
    LOG_INFO("[StatusServer] AllocatePushServer uid={}", req->uid());
    PushServer& server = selectPushServer();
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    resp->set_host(server.host);
    resp->set_port(server.port);
    std::string uid_str = std::to_string(req->uid());
    std::string lock_key = "lock:login:" + uid_str;
    std::string owner = generate_unique_string();

    if (!RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
        LOG_ERROR("[StatusServer] AllocatePushServer: Failed to acquire login lock for uid={}", req->uid());
        resp->set_error(static_cast<int>(ErrorCodes::RPC_ERROR));
        return Status::OK;
    }

    // 临界区: token 读写踢
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

    // 若旧token存在且不同，设置踢人标记通知PushServer关闭旧会话
    if (has_old && !old_token.empty() && old_token != resp->token()) {
        std::string kick_key = "kick:" + uid_str;
        RedisManager::getInstance().setex(kick_key, old_token, 60);
        LOG_INFO("[StatusServer] Set kick marker for uid={}", req->uid());
    }

    RedisManager::getInstance().releaseLock(lock_key, owner);

    // 存储 uid → PushServer 节点映射，用于消息路由 (host:ws_port:grpc_port)
    std::string node_key = "pushnode:" + std::to_string(req->uid());
    RedisManager::getInstance().setex(node_key, server.host + ":" + server.port + ":" + server.grpc_port, 86400);
    LOG_INFO("[StatusServer] Allocated {}:{} (gRPC:{}) token={} for uid={}", server.host, server.port, server.grpc_port, resp->token(), req->uid());
    return Status::OK;
}

Status StatusServiceImpl::ReportLogin(ServerContext* context, const LoginReportReq* request, LoginReportRsp* resp)
{
    auto uid = request->uid();
    auto token = request->token();

    std::string uid_str = std::to_string(uid);
    std::string token_key = USER_TOKEN_PREFIX + uid_str;
    std::string token_value = "";

    bool success = RedisManager::getInstance().get(token_key, token_value);
    // 找不到TOKEN
    if(!success) {
        resp->set_error(static_cast<int>(ErrorCodes::INVALID_UID));
        return Status::OK;
    }
    // TOKEN 不符
    if(token_value != token) {
        resp->set_error(static_cast<int>(ErrorCodes::INVALID_TOKEN));
        return Status::OK;
    }

    // Token 验证成功，递增该节点的连接数（首次登录和重连都走这里）
    if (!request->server_name().empty()) {
        std::lock_guard<std::mutex> lock(server_mtx_);
        auto it = servers_.find(request->server_name());
        if (it != servers_.end()) {
            int idx = it->second.id;
            if (allocate_method_ == "Brute") {
                if (idx > 0 && idx <= server_cnt_ && server_conns_[idx] != INT_MAX)
                    server_conns_[idx]++;
            } else {
                int cur = SegTree_->getVal(idx);
                if (cur != INT_MAX)
                    SegTree_->updateVal(idx, cur + 1);
            }
            LOG_INFO("[StatusServer] ReportLogin: incremented conn count for server={}", request->server_name());
        }
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 断线上报 ----
// PushServer 在用户断开 WebSocket 时调用，递减该节点的连接计数，并清理用户的路由
Status StatusServiceImpl::ReportDisconnect(ServerContext* context, const DisconnectReq* req, DisconnectRsp* resp) {
    // 1. 递减连接数
    {
        std::lock_guard<std::mutex> lock(server_mtx_);
        auto it = servers_.find(req->server_name());
        if (it != servers_.end()) {
            returnServer(it->second);
            LOG_INFO("[StatusServer] ReportDisconnect: decremented conn count for {}", req->server_name());
        } else {
            LOG_ERROR("[StatusServer] ReportDisconnect: unknown server={}", req->server_name());
        }
    }

    // 2. 清理 pushnode 路由（uid > 0 时）
    //    注意：不删 token（utoken_<uid>），因为刷新页面会导致短暂断开重连，
    //    客户端持有 token，删了会导致"登录已过期"。token 由 TTL 自然过期。
    int uid = req->uid();
    if (uid > 0) {
        auto& redis = RedisManager::getInstance();
        std::string uid_str = std::to_string(uid);
        redis.del("pushnode:" + uid_str);
        LOG_INFO("[StatusServer] ReportDisconnect: cleaned pushnode for uid={}", uid);
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

bool StatusServiceImpl::insertToken(int uid, std::string token) {
    std::string uid_str = std::to_string(uid);
    std::string token_key = USER_TOKEN_PREFIX + uid_str;
    // TOKEN过期时间：一天
    // 登陆后超过一天登录失效，需重新登陆
    bool ok = RedisManager::getInstance().setex(token_key, token, 86400);
    LOG_DEBUG("[StatusServer] Token {} inserted for uid = {}!",token_key, uid_str);
    if (!ok) {
        LOG_ERROR("[StatusServer] insertToken FAILED for uid={}: Redis setex returned false", uid);
    }
    return ok;
}


// ---- 日志存储（gRPC handler 和 Kafka consumer 共用） ----
// INFO+ 级别 → LPUSH Redis List + LTRIM 500 + EXPIRE 7天
void StatusServiceImpl::storeLogEntries(const std::string& service, const std::vector<LogEntryData>& entries) {
    auto& redis = RedisManager::getInstance();
    for (const auto& entry : entries) {
        if (entry.level == "DEBUG") continue;  // DEBUG 只写本地文件，不推远程

        std::string json = "{\"service\":\"" + service + "\",\"level\":\"" + entry.level
            + "\",\"message\":\"" + jsonEscape(entry.message) + "\",\"timestamp\":" + std::to_string(entry.timestamp) + "}";

        redis.appendLogAtomic(service, json);
    }
}

// ---- 日志上报（gRPC 兜底通道） ----
Status StatusServiceImpl::ReportLog(ServerContext* context, const ReportLogReq* req, ReportLogRsp* resp) {
    std::vector<LogEntryData> entries;
    for (int i = 0; i < req->entries_size(); ++i) {
        const auto& e = req->entries(i);
        entries.push_back({e.level(), e.message(), e.timestamp()});
    }
    storeLogEntries(req->service(), entries);
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 日志查询 ----
// 教练在客户端查询时 → GateServer HTTP → 此 gRPC → Redis LRANGE
Status StatusServiceImpl::QueryLogs(ServerContext* context, const QueryLogsReq* req, QueryLogsRsp* resp) {
    int limit = req->limit();
    if (limit <= 0 || limit > 500) limit = 500;

    // 动态发现所有 logs:* key（不再硬编码）
    std::vector<std::string> keys;
    auto& redis = RedisManager::getInstance();
    std::string all_keys_json = redis.keys("logs:*");
    
    Json::Reader reader;
    Json::Value key_array;
    if (reader.parse(all_keys_json, key_array) && key_array.isArray()) {
        for (auto& k : key_array)
            keys.push_back(k.asString());
    }
    

    // 如果指定了 service，只留下匹配的 key（前缀匹配）
    if (!req->service().empty()) {
        std::vector<std::string> filtered;
        for (auto& k : keys) {
            // key 格式: "logs:GateServer" 或 "logs:PushServer(127.0.0.1:8890)"
            std::string svc_part = k.substr(5);  // 去掉 "logs:" 前缀
            if (svc_part.compare(0, req->service().size(), req->service()) == 0)
                filtered.push_back(k);
        }
        keys = std::move(filtered);
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    int added = 0;

    const int PER_SERVICE = 100;

    for (auto& key : keys) {
        if (added >= limit) break;
        int fetch = std::min(PER_SERVICE, limit - added);
        auto elements = redis.lrangeVec(key, 0, fetch - 1);
        for (auto& elem : elements) {
            if (added >= limit) break;
            Json::Value obj;
            if (!reader.parse(elem, obj)) continue;  // 跳过损坏的旧条目
            std::string svc = obj.get("service", "").asString();
            std::string lvl = obj.get("level", "").asString();
            if (!req->level().empty() && lvl != req->level()) continue;
            auto* entry = resp->add_entries();
            entry->set_service(svc);
            entry->set_level(lvl);
            entry->set_message(obj.get("message", "").asString());
            entry->set_timestamp(obj.get("timestamp", 0).asInt64());
            added++;
        }
    }
    return Status::OK;
}

// ---- 心跳 ----
// 以 host:port 为唯一 key，同一服务的多个实例（如 PushServer 集群）各自独立
Status StatusServiceImpl::ServerHeartbeat(ServerContext* context, const HeartbeatReq* req, HeartbeatRsp* resp) {
    std::string key = req->host() + ":" + req->port();
    std::lock_guard<std::mutex> lock(server_status_mtx_);
    auto& info = server_status_[key];
    info.service = req->service();
    info.host = req->host();
    info.port = req->port();
    info.last_heartbeat = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    info.status = "online";
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 服务状态查询 ----
Status StatusServiceImpl::QueryServerStatus(ServerContext* context, const QueryServerStatusReq* req, QueryServerStatusRsp* resp) {
    auto now = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    std::vector<std::pair<std::string, ServerHeartbeatInfo>> servers;
    {
        std::lock_guard<std::mutex> lock(server_status_mtx_);
        for (auto& [key, info] : server_status_) {
            int idx = servers_[key].id;
            if (info.last_heartbeat > 0 && (now - info.last_heartbeat) > heartbeat_timeout_secs_) {
                info.status = "offline";
                {
                    std::lock_guard<std::mutex> lock(server_mtx_);
                    if(allocate_method_ == "Brute") {
                        if(idx > 0 and idx <= server_cnt_) server_conns_[idx] = INT_MAX;
                    } else SegTree_->updateVal(idx, INT_MAX);
                }
            } else { // 此时在线，需要检查是不是从离线转为的在线，此时需要更新连接数为0；
                int curconn = 0;
                std::lock_guard<std::mutex> lock(server_mtx_);
                if(allocate_method_ == "Brute") {
                    if(idx > 0 and idx <= server_cnt_) curconn = server_conns_[idx];
                } else curconn = SegTree_->getVal(idx);
                if(curconn == INT_MAX) {
                    if(allocate_method_ == "Brute") {
                        if(idx > 0 and idx <= server_cnt_) server_conns_[idx] = 0;
                    } else SegTree_->updateVal(idx, 0);
                }
            }
            servers.push_back({key, info});
        }
    }

    for (auto& [key, info] : servers) {
        auto* srv = resp->add_servers();
        srv->set_service(info.service);
        srv->set_host(info.host);
        srv->set_port(info.port);
        srv->set_status(info.status);
        srv->set_last_heartbeat(info.last_heartbeat);

        // PushServer 节点附加当前连接数（以 host:port 匹配负载均衡器数据）
        int conns = 0;
        {
            std::lock_guard<std::mutex> lock(server_mtx_);
            std::string lb_key = info.host + ":" + info.port;
            auto it = servers_.find(lb_key);
            if (it != servers_.end()) {
                int id = it->second.id;
                if (allocate_method_ == "Brute") {
                    if (id > 0 and id <= server_cnt_) conns = server_conns_[id];
                } else {
                    conns = SegTree_->getVal(id);
                }
            }
        }
        srv->set_connections(conns);
    }

    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}

// ---- 查询用户所在推送节点 ----
Status StatusServiceImpl::GetPushServerForUser(ServerContext* context, const GetPushServerForUserReq* req,
                                                GetPushServerForUserRsp* resp) {
    auto& redis = RedisManager::getInstance();
    for (int i = 0; i < req->uids_size(); ++i) {
        int uid = req->uids(i);
        std::string node_key = "pushnode:" + std::to_string(uid);
        std::string node_value;
        auto* node = resp->add_nodes();
        node->set_uid(uid);
        if (redis.get(node_key, node_value) && !node_value.empty()) {
            auto colon1 = node_value.find(':');
            auto colon2 = node_value.find(':', colon1 + 1);
            if (colon1 != std::string::npos) {
                node->set_host(node_value.substr(0, colon1));
                if (colon2 != std::string::npos) {
                    node->set_port(node_value.substr(colon1 + 1, colon2 - colon1 - 1));
                    node->set_grpc_port(node_value.substr(colon2 + 1));
                } else {
                    node->set_port(node_value.substr(colon1 + 1));
                }
            }
            node->set_online(true);
        } else {
            node->set_online(false);
        }
    }
    resp->set_error(static_cast<int>(ErrorCodes::SUCCESS));
    return Status::OK;
}
