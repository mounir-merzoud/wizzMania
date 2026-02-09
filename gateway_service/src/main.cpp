#include "auth.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <google/protobuf/util/json_util.h>

#include "httplib.h"

#include <iostream>
#include <cstdlib>
#include <string>

namespace {
using google::protobuf::util::JsonParseOptions;
using google::protobuf::util::JsonPrintOptions;
using google::protobuf::util::MessageToJsonString;
using google::protobuf::util::JsonStringToMessage;

std::string json_error(const std::string& message) {
    return std::string("{\"success\":false,\"message\":\"") + message + "\"}";
}
}

int main(int /*argc*/, char** /*argv*/) {
    const std::string http_listen_host = "0.0.0.0";
    const int http_port = 8080;

    const char* envTarget = std::getenv("AUTH_GRPC_TARGET");
    const std::string auth_target = (envTarget && *envTarget) ? std::string(envTarget) : std::string("localhost:50051");
    auto authChannel = grpc::CreateChannel(auth_target, grpc::InsecureChannelCredentials());
    auto authStub = securecloud::auth::AuthService::NewStub(authChannel);

    httplib::Server server;

    server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
        res.status = 200;
        res.set_content("{\"ok\":true}", "application/json");
    });

    // POST /login
    // Body JSON: {"username":"...","password":"..."}
    // Response JSON: auth_service LoginResponse (tokens, expires)
    server.Post("/login", [&](const httplib::Request& req, httplib::Response& res) {
        if (req.body.empty()) {
            res.status = 400;
            res.set_content(json_error("Empty request body"), "application/json");
            return;
        }

        securecloud::auth::LoginRequest authReq;
        JsonParseOptions parseOpts;
        parseOpts.ignore_unknown_fields = true;

        auto parseStatus = JsonStringToMessage(req.body, &authReq, parseOpts);
        if (!parseStatus.ok()) {
            res.status = 400;
            res.set_content(json_error("Invalid JSON payload"), "application/json");
            return;
        }

        grpc::ClientContext authCtx;
        securecloud::auth::LoginResponse authResp;
        auto callStatus = authStub->Login(&authCtx, authReq, &authResp);

        if (!callStatus.ok()) {
            // Auth refused or service error
            res.status = 401;
            res.set_content(json_error(callStatus.error_message()), "application/json");
            return;
        }

        std::string json;
        JsonPrintOptions printOpts;
        printOpts.add_whitespace = false;
        auto printStatus = MessageToJsonString(authResp, &json, printOpts);

        if (!printStatus.ok()) {
            res.status = 500;
            res.set_content(json_error("Failed to serialize response"), "application/json");
            return;
        }

        res.status = 200;
        res.set_content(json, "application/json");
    });

    std::cout << "HTTP Gateway listening on http://" << http_listen_host << ":" << http_port << "\n";
    std::cout << "Proxying AuthService gRPC at " << auth_target << "\n";
    server.listen(http_listen_host, http_port);
    return 0;
}
