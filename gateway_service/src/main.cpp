#include "gateway.pb.h"
#include "auth.grpc.pb.h"
#include "messaging.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/json_util.h>

#include "httplib.h"

#include <algorithm>
#include <ctime>
#include <iostream>
#include <cstdlib>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::JsonPrintOptions;
using google::protobuf::util::MessageToJsonString;
using google::protobuf::util::JsonStringToMessage;

JsonPrintOptions compact_json_opts() {
    JsonPrintOptions opts;
    opts.add_whitespace = false;
    return opts;
}

void set_json(httplib::Response& res, int status, const std::string& body) {
    res.status = status;
    res.set_content(body, "application/json");
}

template <typename TProto>
bool set_proto_json(httplib::Response& res, int status, const TProto& message) {
    std::string json;
    const auto printStatus = MessageToJsonString(message, &json, compact_json_opts());
    if (!printStatus.ok()) {
        set_json(res, 500, json_error("Failed to serialize response"));
        return false;
    }
    set_json(res, status, json);
    return true;
}

std::string json_error(const std::string& message) {
    return std::string("{\"success\":false,\"message\":\"") + message + "\"}";
}

std::string get_bearer_token(const httplib::Request& req) {
    if (!req.has_header("Authorization")) {
        return {};
    }
    const auto value = req.get_header_value("Authorization");
    const std::string prefix = "Bearer ";
    if (value.size() <= prefix.size()) {
        return {};
    }
    if (value.rfind(prefix, 0) != 0) {
        return {};
    }
    return value.substr(prefix.size());
}

bool validate_access_token(securecloud::auth::AuthService::Stub& authStub,
                           const httplib::Request& req,
                           securecloud::auth::ValidateTokenResponse* out,
                           std::string* err) {
    if (!out) {
        if (err) *err = "Internal error";
        return false;
    }
    const auto token = get_bearer_token(req);
    if (token.empty()) {
        if (err) *err = "Missing Authorization: Bearer <token>";
        return false;
    }

    securecloud::auth::ValidateTokenRequest vreq;
    vreq.set_access_token(token);
    grpc::ClientContext ctx;
    auto st = authStub.ValidateToken(&ctx, vreq, out);
    if (!st.ok()) {
        if (err) *err = st.error_message();
        return false;
    }
    if (!out->valid()) {
        if (err) *err = "Invalid token";
        return false;
    }
    return true;
}

bool try_parse_int64(const std::string& s, long long* out) {
    if (!out) return false;
    if (s.empty()) return false;
    try {
        size_t idx = 0;
        const long long v = std::stoll(s, &idx);
        if (idx != s.size()) return false;
        *out = v;
        return true;
    } catch (...) {
        return false;
    }
}

std::string dm_conversation_key(const std::string& userA, const std::string& userB) {
    long long a = 0, b = 0;
    if (try_parse_int64(userA, &a) && try_parse_int64(userB, &b)) {
        if (a > b) std::swap(a, b);
        return "dm:" + std::to_string(a) + ":" + std::to_string(b);
    }

    // Fallback: stable lexical order.
    if (userA <= userB) return "dm:" + userA + ":" + userB;
    return "dm:" + userB + ":" + userA;
}
}

int main(int /*argc*/, char** /*argv*/) {
    const std::string http_listen_host = "0.0.0.0";
    const int http_port = 8080;

    const char* envTarget = std::getenv("AUTH_GRPC_TARGET");
    const std::string auth_target = (envTarget && *envTarget) ? std::string(envTarget) : std::string("localhost:50051");
    auto authChannel = grpc::CreateChannel(auth_target, grpc::InsecureChannelCredentials());
    auto authStub = securecloud::auth::AuthService::NewStub(authChannel);

    const char* envMessaging = std::getenv("MESSAGING_GRPC_TARGET");
    const std::string messaging_target = (envMessaging && *envMessaging) ? std::string(envMessaging) : std::string("localhost:7002");
    auto messagingChannel = grpc::CreateChannel(messaging_target, grpc::InsecureChannelCredentials());
    auto messagingStub = securecloud::messaging::MessagingService::NewStub(messagingChannel);

    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        set_json(res, 200, "{\"ok\":true}");
    });

    // GET /me (Authorization: Bearer <access_token>)
    server.Get("/me", [&](const httplib::Request& req, httplib::Response& res) {
        securecloud::auth::ValidateTokenResponse vresp;
        std::string err;
        if (!validate_access_token(*authStub, req, &vresp, &err)) {
            set_json(res, 401, json_error(err));
            return;
        }

        set_proto_json(res, 200, vresp);
    });

    // GET /users (Authorization: Bearer <access_token>)
    // Returns JSON (ListUsersHttpResponse)
    server.Get("/users", [&](const httplib::Request& req, httplib::Response& res) {
        securecloud::auth::ValidateTokenResponse vresp;
        std::string err;
        if (!validate_access_token(*authStub, req, &vresp, &err)) {
            set_json(res, 401, json_error(err));
            return;
        }

        securecloud::auth::ListUsersRequest lreq;
        lreq.set_access_token(get_bearer_token(req));
        lreq.set_include_self(false);
        securecloud::auth::ListUsersResponse lresp;
        grpc::ClientContext ctx;
        auto st = authStub->ListUsers(&ctx, lreq, &lresp);
        if (!st.ok()) {
            set_json(res, 502, json_error(st.error_message()));
            return;
        }

        securecloud::gateway::ListUsersHttpResponse out;
        out.mutable_users()->Reserve(lresp.users_size());
        for (const auto& u : lresp.users()) {
            auto* hu = out.add_users();
            hu->set_user_id(u.user_id());
            hu->set_full_name(u.full_name());
            hu->set_email(u.email());
            hu->set_role(u.role());
        }

        set_proto_json(res, 200, out);
    });

    // POST /login
    // Body JSON: {"username":"...","password":"..."}
    // Response JSON: auth_service LoginResponse (tokens, expires)
    server.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            set_json(res, 400, json_error("Empty request body"));
            return;
        }

        securecloud::auth::LoginRequest authReq;
        JsonParseOptions parseOpts;
        parseOpts.ignore_unknown_fields = true;

        auto parseStatus = JsonStringToMessage(req.body, &authReq, parseOpts);
        if (!parseStatus.ok()) {
            set_json(res, 400, json_error("Invalid JSON payload"));
            return;
        }

        grpc::ClientContext authCtx;
        securecloud::auth::LoginResponse authResp;
        auto callStatus = authStub->Login(&authCtx, authReq, &authResp);

        if (!callStatus.ok()) {
            // Auth refused or service error
            set_json(res, 401, json_error(callStatus.error_message()));
            return;
        }

        set_proto_json(res, 200, authResp);
    });

    // POST /register (admin only)
    // Body JSON: {"fullName":"...", "email":"...", "password":"...", "roleName":"user"}
    server.Post("/register", [&](const httplib::Request& req, httplib::Response& res) {
        securecloud::auth::ValidateTokenResponse vresp;
        std::string err;
        if (!validate_access_token(*authStub, req, &vresp, &err)) {
            set_json(res, 401, json_error(err));
            return;
        }
        if (req.body.empty()) {
            set_json(res, 400, json_error("Empty request body"));
            return;
        }

        securecloud::gateway::RegisterHttpRequest in;
        JsonParseOptions parseOpts;
        parseOpts.ignore_unknown_fields = true;
        auto parseStatus = JsonStringToMessage(req.body, &in, parseOpts);
        if (!parseStatus.ok()) {
            set_json(res, 400, json_error("Invalid JSON payload"));
            return;
        }

        securecloud::auth::RegisterRequest rreq;
        rreq.set_full_name(in.full_name());
        rreq.set_email(in.email());
        rreq.set_password(in.password());
        rreq.set_admin_token(get_bearer_token(req));
        if (!in.role_name().empty()) {
            rreq.set_role_name(in.role_name());
        }

        securecloud::auth::RegisterResponse rresp;
        grpc::ClientContext ctx;
        auto st = authStub->RegisterUser(&ctx, rreq, &rresp);
        if (!st.ok()) {
            const int status = (st.error_code() == grpc::StatusCode::PERMISSION_DENIED) ? 403 : 502;
            set_json(res, status, json_error(st.error_message()));
            return;
        }

        set_proto_json(res, 200, rresp);
    });

    // GET /conversations
    // Returns JSON (ListConversationsResponse)
    server.Get("/conversations", [&](const httplib::Request& req, httplib::Response& res) {
        // optional auth: if present, validate; otherwise allow (dev-friendly)
        if (req.has_header("Authorization")) {
            securecloud::auth::ValidateTokenResponse vresp;
            std::string err;
            if (!validate_access_token(*authStub, req, &vresp, &err)) {
                res.status = 401;
                res.set_content(json_error(err), "application/json");
                return;
            }
        }

        securecloud::messaging::HistoryRequest hreq;
        hreq.set_limit(200);
        securecloud::messaging::HistoryResponse hresp;
        grpc::ClientContext ctx;
        auto st = messagingStub->GetHistory(&ctx, hreq, &hresp);
        if (!st.ok()) {
            res.status = 502;
            res.set_content(json_error(st.error_message()), "application/json");
            return;
        }

        // pick most recent message per conversation
        std::unordered_map<std::string, securecloud::messaging::EncryptedMessage> lastByConv;
        lastByConv.reserve(static_cast<size_t>(hresp.messages_size()));

        for (const auto& m : hresp.messages()) {
            const std::string& cid = m.conversation_id();
            if (cid.empty()) continue;
            if (lastByConv.find(cid) == lastByConv.end()) {
                lastByConv.emplace(cid, m);
            }
        }

        securecloud::gateway::ListConversationsResponse out;
        out.mutable_conversations()->Reserve(static_cast<int>(lastByConv.size()));
        for (const auto& kv : lastByConv) {
            const auto& msg = kv.second;
            auto* c = out.add_conversations();
            c->set_conversation_id(kv.first);
            c->set_last_message(std::string(msg.ciphertext().begin(), msg.ciphertext().end()));
            c->set_last_timestamp_unix(msg.timestamp_unix());
        }

        std::string json;
        JsonPrintOptions printOpts;
        printOpts.add_whitespace = false;
        auto pst = MessageToJsonString(out, &json, printOpts);
        if (!pst.ok()) {
            res.status = 500;
            res.set_content(json_error("Failed to serialize response"), "application/json");
            return;
        }

        res.status = 200;
        res.set_content(json, "application/json");
    });

    // GET /conversations/:id/messages?limit=50
    server.Get(R"(/conversations/([^/]+)/messages)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto conversationId = req.matches.size() >= 2 ? req.matches[1].str() : std::string();
        if (conversationId.empty()) {
            res.status = 400;
            res.set_content(json_error("Missing conversation id"), "application/json");
            return;
        }

        std::string conversationKey = conversationId;

        // optional auth
        if (req.has_header("Authorization")) {
            securecloud::auth::ValidateTokenResponse vresp;
            std::string err;
            if (!validate_access_token(*authStub, req, &vresp, &err)) {
                res.status = 401;
                res.set_content(json_error(err), "application/json");
                return;
            }

            // Map UI contact-id-style paths into a stable per-pair conversation key.
            conversationKey = dm_conversation_key(vresp.user_id(), conversationId);
        }

        int limit = 50;
        if (req.has_param("limit")) {
            try {
                limit = std::max(1, std::min(500, std::stoi(req.get_param_value("limit"))));
            } catch (...) {
                // ignore
            }
        }

        securecloud::messaging::HistoryRequest hreq;
        hreq.set_conversation_id(conversationKey);
        hreq.set_limit(limit);
        securecloud::messaging::HistoryResponse hresp;
        grpc::ClientContext ctx;
        auto st = messagingStub->GetHistory(&ctx, hreq, &hresp);
        if (!st.ok()) {
            res.status = 502;
            res.set_content(json_error(st.error_message()), "application/json");
            return;
        }

        securecloud::gateway::ListMessagesResponse out;

        // hresp is newest-first; return oldest-first
        for (int i = hresp.messages_size() - 1; i >= 0; --i) {
            const auto& m = hresp.messages(i);
            auto* hm = out.add_messages();
            hm->set_message_id(m.message_id());
            hm->set_conversation_id(conversationId);
            hm->set_sender_id(m.sender_id());
            hm->set_content(std::string(m.ciphertext().begin(), m.ciphertext().end()));
            hm->set_timestamp_unix(m.timestamp_unix());
        }

        std::string json;
        JsonPrintOptions printOpts;
        printOpts.add_whitespace = false;
        auto pst = MessageToJsonString(out, &json, printOpts);
        if (!pst.ok()) {
            res.status = 500;
            res.set_content(json_error("Failed to serialize response"), "application/json");
            return;
        }

        res.status = 200;
        res.set_content(json, "application/json");
    });

    // POST /conversations/:id/messages
    // Body JSON: {"content":"..."}
    server.Post(R"(/conversations/([^/]+)/messages)", [&](const httplib::Request& req, httplib::Response& res) {
        const auto conversationId = req.matches.size() >= 2 ? req.matches[1].str() : std::string();
        if (conversationId.empty()) {
            res.status = 400;
            res.set_content(json_error("Missing conversation id"), "application/json");
            return;
        }
        if (req.body.empty()) {
            res.status = 400;
            res.set_content(json_error("Empty request body"), "application/json");
            return;
        }

        securecloud::auth::ValidateTokenResponse vresp;
        std::string err;
        if (!validate_access_token(*authStub, req, &vresp, &err)) {
            res.status = 401;
            res.set_content(json_error(err), "application/json");
            return;
        }

        securecloud::gateway::SendMessageHttpRequest in;
        JsonParseOptions parseOpts;
        parseOpts.ignore_unknown_fields = true;
        auto parseStatus = JsonStringToMessage(req.body, &in, parseOpts);
        if (!parseStatus.ok()) {
            res.status = 400;
            res.set_content(json_error("Invalid JSON payload"), "application/json");
            return;
        }
        if (in.content().empty()) {
            res.status = 400;
            res.set_content(json_error("Missing content"), "application/json");
            return;
        }

        securecloud::messaging::EncryptedMessage msg;
        msg.set_conversation_id(dm_conversation_key(vresp.user_id(), conversationId));
        msg.set_sender_id(vresp.user_id());
        msg.set_ciphertext(in.content());
        msg.set_timestamp_unix(std::time(nullptr));

        securecloud::messaging::SendAck ack;
        grpc::ClientContext ctx;
        auto st = messagingStub->SendMessage(&ctx, msg, &ack);
        if (!st.ok()) {
            res.status = 502;
            res.set_content(json_error(st.error_message()), "application/json");
            return;
        }

        securecloud::gateway::SendMessageHttpResponse out;
        out.set_message_id(ack.message_id());
        out.set_accepted(ack.accepted());

        std::string json;
        JsonPrintOptions printOpts;
        printOpts.add_whitespace = false;
        auto pst = MessageToJsonString(out, &json, printOpts);
        if (!pst.ok()) {
            res.status = 500;
            res.set_content(json_error("Failed to serialize response"), "application/json");
            return;
        }

        res.status = 200;
        res.set_content(json, "application/json");
    });

    std::cout << "HTTP Gateway listening on http://" << http_listen_host << ":" << http_port << "\n";
    std::cout << "Proxying AuthService gRPC at " << auth_target << "\n";
    std::cout << "Proxying MessagingService gRPC at " << messaging_target << "\n";
    server.listen(http_listen_host, http_port);
    return 0;
}
