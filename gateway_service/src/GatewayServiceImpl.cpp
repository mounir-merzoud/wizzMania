#include "gateway.grpc.pb.h"
#include "auth.grpc.pb.h"
#include <grpcpp/grpcpp.h>
#include <iostream>

using grpc::ServerContext;
using grpc::Status;

class GatewayServiceImpl final : public securecloud::gateway::GatewayService::Service {
public:
    explicit GatewayServiceImpl(
        std::shared_ptr<securecloud::auth::AuthService::Stub> authStub)
        : authStub_(authStub) {}

    Status Login(ServerContext* context,
                 const securecloud::gateway::LoginRequest* request,
                 securecloud::gateway::LoginResponse* response) override {
        securecloud::auth::LoginRequest authReq;
        authReq.set_username(request->username());
        authReq.set_password(request->password());

        securecloud::auth::LoginResponse authResp;
        grpc::ClientContext authCtx;
        auto status = authStub_->Login(&authCtx, authReq, &authResp);

        if (!status.ok()) return status;

        response->set_access_token(authResp.access_token());
        response->set_refresh_token(authResp.refresh_token());
        response->set_expires_in(authResp.expires_in());
        return Status::OK;
    }

private:
    std::shared_ptr<securecloud::auth::AuthService::Stub> authStub_;
};
