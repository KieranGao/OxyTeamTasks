#include "PushGrpcServiceImpl.h"
#include "MainServer.h"
#include "RedisManager.h"
#include "MySQLManager.h"
#include "Logger.h"
#include "AsyncTaskPool.h"
#include "ConfigManager.h"
#include "RPCConnectPool.h"
#include <json/json.h>
#include "boost/uuid/uuid.hpp"
#include "boost/uuid/random_generator.hpp"
#include "boost/uuid/uuid_io.hpp"

static MainServer* g_main_server = nullptr;
static std::string g_grpc_port;  // 本节点 gRPC 端口

void setGlobalMainServer(MainServer* server) { g_main_server = server; }
void setGlobalGrpcPort(const std::string& port) { g_grpc_port = port; }

// 解析 pushnode 值 "host:ws_port:grpc_port"
static bool parseNodeValue(const std::string& val, std::string& host, std::string& ws_port, std::string& grpc_port) {
    auto c1 = val.find(':');
    if (c1 == std::string::npos) return false;
    auto c2 = val.find(':', c1 + 1);
    if (c2 == std::string::npos) return false;
    host = val.substr(0, c1);
    ws_port = val.substr(c1 + 1, c2 - c1 - 1);
    grpc_port = val.substr(c2 + 1);
    return !host.empty() && !grpc_port.empty();
}

static std::string generate_lock_owner() {
    boost::uuids::uuid uuid = boost::uuids::random_generator()();
    return boost::uuids::to_string(uuid);
}

// Helper: write message to MySQL and Redis, then try WebSocket push (local or cross-node)
static bool pushMessageToUser(int uid, const std::string& msgType,
                              const std::string& title, const std::string& payload,
                              bool* delivered = nullptr) {
    // 1. Write to MySQL
    MySQLManager::getInstance().insertMessage(uid, msgType, title, payload);

    // 2. Push to Redis cache
    std::string uid_str = std::to_string(uid);
    Json::Value msg;
    msg["msg_type"] = msgType;
    msg["title"] = title;
    msg["payload"] = payload;
    msg["is_read"] = 0;
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    char time_buf[64];
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", localtime(&time_t_now));
    msg["created_at"] = time_buf;

    std::string msg_json = msg.toStyledString();

    // Atomic: LPUSH + LTRIM + EXPIRE + INCR, protected by distributed lock
    std::string lock_key = "lock:unread:" + uid_str;
    std::string owner = generate_lock_owner();
    if (RedisManager::getInstance().acquireLockWithRetry(lock_key, owner, 10)) {
        RedisManager::getInstance().pushMessageAtomic(uid_str, msg_json);
        RedisManager::getInstance().releaseLock(lock_key, owner);
    } else {
        LOG_ERROR("[Push] Failed to acquire unread lock for pushMessage uid={}", uid_str);
    }

    // 3. Try local WebSocket push
    if (g_main_server) {
        auto session = g_main_server->getSessionByUid(uid);
        if (session) {
            Json::Value ws_msg;
            ws_msg["type"] = WS_MSG_NOTIFY;
            ws_msg["msg_type"] = msgType;
            ws_msg["title"] = title;
            ws_msg["payload"] = payload;
            session->send(ws_msg.toStyledString());
            if (delivered) *delivered = true;
            return true;
        }
    }

    // 4. User not local — try cross-node forwarding
    std::string node_val;
    if (RedisManager::getInstance().get("pushnode:" + uid_str, node_val) && !node_val.empty()) {
        std::string host, ws_port, target_grpc;
        if (parseNodeValue(node_val, host, ws_port, target_grpc) && target_grpc != g_grpc_port) {
            // Forward to target PushServer node
            try {
                auto stub = PushNodeManager::getInstance().getStub(host, target_grpc);
                if (stub) {
                    grpc::ClientContext ctx;
                    ctx.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(5));
                    PushToUserReq fwd_req;
                    PushToUserRsp fwd_rsp;
                    fwd_req.set_uid(uid);
                    fwd_req.set_msg_type(msgType);
                    fwd_req.set_title(title);
                    fwd_req.set_payload(payload);
                    grpc::Status s = stub->PushToUser(&ctx, fwd_req, &fwd_rsp);
                    PushNodeManager::getInstance().returnStub(host, target_grpc, std::move(stub));
                    if (s.ok() && fwd_rsp.error() == 0) {
                        if (delivered) *delivered = fwd_rsp.delivered();
                        LOG_INFO("[Push] Forwarded PushToUser uid={} to {}:{}", uid, host, target_grpc);
                        return true;
                    }
                    LOG_ERROR("[Push] Forward failed for uid={}: {}", uid, s.ok() ? "remote error" : s.error_message());
                }
            } catch (const std::exception& e) {
                LOG_ERROR("[Push] Forward exception for uid={}: {}", uid, e.what());
            }
        }
    }

    if (delivered) *delivered = false;
    return true;
}

Status PushGrpcServiceImpl::PushToUser(ServerContext* context, const PushToUserReq* req,
                                        PushToUserRsp* resp) {
    int uid = req->uid();
    std::string msgType = req->msg_type();
    std::string title = req->title();
    std::string payload = req->payload();

    LOG_INFO("[Push] PushToUser uid={} type={} title=\"{}\"", uid, msgType, title);

    bool delivered = false;
    bool ok = pushMessageToUser(uid, msgType, title, payload, &delivered);

    resp->set_error(ok ? 0 : static_cast<int>(ErrorCodes::RPC_ERROR));
    resp->set_delivered(delivered);
    return Status::OK;
}

Status PushGrpcServiceImpl::PushToTeam(ServerContext* context, const PushToTeamReq* req,
                                        PushToTeamRsp* resp) {
    int teamId = req->team_id();
    int excludeUid = req->exclude_uid();
    std::string msgType = req->msg_type();
    std::string title = req->title();
    std::string payload = req->payload();

    LOG_INFO("[Push] PushToTeam team_id={} type={} title=\"{}\"", teamId, msgType, title);

    auto uids = MySQLManager::getInstance().getUsersByTeam(teamId);
    int delivered_count = 0;
    int cached_count = 0;

    for (int uid : uids) {
        if (uid == excludeUid) continue;
        bool delivered = false;
        pushMessageToUser(uid, msgType, title, payload, &delivered);
        if (delivered) delivered_count++;
        else cached_count++;
    }

    resp->set_error(0);
    resp->set_delivered_count(delivered_count);
    resp->set_cached_count(cached_count);
    return Status::OK;
}

Status PushGrpcServiceImpl::GetMessages(ServerContext* context, const GetMessagesReq* req,
                                         GetMessagesRsp* resp) {
    int uid = req->uid();
    int page = req->page() > 0 ? req->page() : 1;
    int pageSize = req->page_size() > 0 ? req->page_size() : 20;
    int offset = (page - 1) * pageSize;

    LOG_INFO("[Push] GetMessages uid={} page={} pageSize={}", uid, page, pageSize);

    // Get unread count from Redis
    std::string uid_str = std::to_string(uid);
    std::string unread_str;
    long unread_count = 0;
    if (RedisManager::getInstance().get("unread:" + uid_str, unread_str)) {
        try { unread_count = std::stol(unread_str); } catch (...) { unread_count = 0; }
    }
    resp->set_unread_count(static_cast<int32_t>(unread_count));

    // For now, return messages from Redis cache (limited to 50 recent)
    // Full MySQL query will be added when MySQLDao is extended
    std::vector<std::string> cached;
    RedisManager::getInstance().lrange("msgs:" + uid_str, 0, 49, cached);

    int total = static_cast<int>(cached.size());
    resp->set_total(total);

    int count = 0;
    for (int i = offset; i < total && count < pageSize; ++i, ++count) {
        Json::Value msg;
        Json::Reader reader;
        if (reader.parse(cached[i], msg)) {
            auto* item = resp->add_messages();
            item->set_id(0);  // No real ID from Redis cache
            item->set_msg_type(msg.get("msg_type", "").asString());
            item->set_title(msg.get("title", "").asString());
            item->set_content(msg.get("payload", "").asString());
            item->set_is_read(msg.get("is_read", 0).asInt());
            item->set_created_at(msg.get("created_at", "").asString());
        }
    }

    resp->set_error(0);
    return Status::OK;
}

Status PushGrpcServiceImpl::MarkRead(ServerContext* context, const MarkReadReq* req,
                                      MarkReadRsp* resp) {
    int uid = req->uid();
    LOG_INFO("[Push] MarkRead uid={} ids_count={}", uid, req->ids_size());

    std::vector<int64_t> ids;
    for (int i = 0; i < req->ids_size(); ++i) ids.push_back(req->ids(i));

    // Update MySQL
    MySQLManager::getInstance().markMessagesRead(uid, ids);

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

    resp->set_error(0);
    return Status::OK;
}

Status PushGrpcServiceImpl::DeleteMessage(ServerContext* context, const DeleteMessageReq* req,
                                           DeleteMessageRsp* resp) {
    int uid = req->uid();
    LOG_INFO("[Push] DeleteMessage uid={} ids_count={}", uid, req->ids_size());

    std::vector<int64_t> ids;
    for (int i = 0; i < req->ids_size(); ++i) ids.push_back(req->ids(i));

    MySQLManager::getInstance().deleteMessages(uid, ids);

    resp->set_error(0);
    return Status::OK;
}
