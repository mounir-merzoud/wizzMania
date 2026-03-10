/**
 * @file test_auth_service.cpp
 * @brief Tests unitaires pour AuthServiceImpl
 *
 * Couvre :
 *  - Login  : succès / utilisateur inexistant / mauvais mot de passe
 *  - RegisterUser : succès / champs manquants / email déjà existant / token admin manquant
 *  - ValidateToken : token valide / token invalide
 *  - ListUsers : succès / token invalide
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <memory>

#include "service/AuthServiceImpl.h"
#include "mocks/MockDatabase.h"
#include "mocks/MockAuthManager.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Crée un ServerContext factice (non-null, mais non connecté). */
static grpc::ServerContext* fakeCtx() {
    static grpc::ServerContext ctx;
    return &ctx;
}

/** Construit un UserRecord de test. */
static UserRecord makeUser(int id = 1,
                           const std::string& email = "alice@example.com",
                           const std::string& hash  = "$2b$12$FAKEHASH",
                           const std::string& role  = "user") {
    UserRecord u;
    u.id            = id;
    u.email         = email;
    u.password_hash = hash;
    u.full_name     = "Alice";
    u.role_id       = 1;
    u.role_name     = role;
    return u;
}

// ---------------------------------------------------------------------------
// Fixture commune
// ---------------------------------------------------------------------------

class AuthServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_  = std::make_shared<NiceMock<MockDatabase>>();
        mgr_ = std::make_shared<NiceMock<MockAuthManager>>();
        svc_ = std::make_unique<AuthServiceImpl>(mgr_, db_);
    }

    std::shared_ptr<NiceMock<MockDatabase>>    db_;
    std::shared_ptr<NiceMock<MockAuthManager>> mgr_;
    std::unique_ptr<AuthServiceImpl>            svc_;
};

// ===========================================================================
// LOGIN
// ===========================================================================

TEST_F(AuthServiceTest, Login_Success) {
    const std::string fakeToken = "jwt.access.token";
    UserRecord user = makeUser();

    EXPECT_CALL(*db_, getUserByEmail("alice@example.com"))
        .WillOnce(Return(std::optional<UserRecord>(user)));

    // getUserPermissions renvoie deux permissions
    EXPECT_CALL(*db_, getUserPermissions(1))
        .WillOnce(Return(std::vector<std::string>{"user.read", "user.update"}));

    // generateTokenWithRole renvoie un token factice
    EXPECT_CALL(*mgr_, generateTokenWithRole("1", "alice@example.com", "user", _))
        .WillOnce(Return(fakeToken));

    securecloud::auth::LoginRequest req;
    req.set_username("alice@example.com");
    req.set_password("correct_password");   // PasswordHasher::verifyPassword doit passer

    securecloud::auth::LoginResponse resp;
    auto status = svc_->Login(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.access_token(), fakeToken);
    EXPECT_EQ(resp.expires_in(), 86400);
    EXPECT_EQ(resp.role(), "user");
    EXPECT_EQ(resp.permissions_size(), 2);
}

TEST_F(AuthServiceTest, Login_UserNotFound) {
    EXPECT_CALL(*db_, getUserByEmail("unknown@example.com"))
        .WillOnce(Return(std::nullopt));

    securecloud::auth::LoginRequest req;
    req.set_username("unknown@example.com");
    req.set_password("anypassword");

    securecloud::auth::LoginResponse resp;
    auto status = svc_->Login(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::NOT_FOUND);
}

TEST_F(AuthServiceTest, Login_WrongPassword) {
    // hash qui ne correspond pas à "wrong_password"
    UserRecord user = makeUser(1, "alice@example.com", "$2b$12$HASH_FOR_CORRECT");

    EXPECT_CALL(*db_, getUserByEmail("alice@example.com"))
        .WillOnce(Return(std::optional<UserRecord>(user)));

    securecloud::auth::LoginRequest req;
    req.set_username("alice@example.com");
    req.set_password("wrong_password");

    securecloud::auth::LoginResponse resp;
    auto status = svc_->Login(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
}

TEST_F(AuthServiceTest, Login_EmptyCredentials) {
    // getUserByEmail ne doit pas être appelé si le username est vide
    EXPECT_CALL(*db_, getUserByEmail(_)).Times(0);

    securecloud::auth::LoginRequest req;
    // username et password vides => comportement défensif de la couche
    req.set_username("");
    req.set_password("");

    securecloud::auth::LoginResponse resp;
    auto status = svc_->Login(fakeCtx(), &req, &resp);

    // Soit NOT_FOUND (getUserByEmail("") → nullopt), soit UNAUTHENTICATED
    EXPECT_FALSE(status.ok());
}

// ===========================================================================
// REGISTER USER
// ===========================================================================

TEST_F(AuthServiceTest, RegisterUser_Success) {
    // Préparer un token admin valide
    TokenInfo adminInfo;
    adminInfo.valid  = true;
    adminInfo.role   = "admin";
    adminInfo.userId = "99";
    adminInfo.email  = "admin@example.com";

    EXPECT_CALL(*mgr_, decodeToken("valid.admin.token"))
        .WillOnce(Return(adminInfo));

    EXPECT_CALL(*db_, userExists("newuser@example.com"))
        .WillOnce(Return(false));

    EXPECT_CALL(*db_, registerUserWithRole("New User", "newuser@example.com", _, "user"))
        .Times(1);

    securecloud::auth::RegisterRequest req;
    req.set_full_name("New User");
    req.set_email("newuser@example.com");
    req.set_password("StrongP@ss1");
    req.set_admin_token("valid.admin.token");

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(resp.success());
}

TEST_F(AuthServiceTest, RegisterUser_MissingAdminToken) {
    securecloud::auth::RegisterRequest req;
    req.set_full_name("Bob");
    req.set_email("bob@example.com");
    req.set_password("password");
    // admin_token non renseigné

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::PERMISSION_DENIED);
    EXPECT_FALSE(resp.success());
}

TEST_F(AuthServiceTest, RegisterUser_InvalidAdminToken) {
    TokenInfo invalidInfo;
    invalidInfo.valid = false;

    EXPECT_CALL(*mgr_, decodeToken("bad.token"))
        .WillOnce(Return(invalidInfo));

    securecloud::auth::RegisterRequest req;
    req.set_full_name("Bob");
    req.set_email("bob@example.com");
    req.set_password("password");
    req.set_admin_token("bad.token");

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::PERMISSION_DENIED);
}

TEST_F(AuthServiceTest, RegisterUser_EmailAlreadyExists) {
    TokenInfo adminInfo;
    adminInfo.valid = true;
    adminInfo.role  = "admin";

    EXPECT_CALL(*mgr_, decodeToken("valid.admin.token"))
        .WillOnce(Return(adminInfo));

    EXPECT_CALL(*db_, userExists("existing@example.com"))
        .WillOnce(Return(true));

    securecloud::auth::RegisterRequest req;
    req.set_full_name("Alice");
    req.set_email("existing@example.com");
    req.set_password("password");
    req.set_admin_token("valid.admin.token");

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::ALREADY_EXISTS);
    EXPECT_FALSE(resp.success());
}

TEST_F(AuthServiceTest, RegisterUser_MissingFields) {
    TokenInfo adminInfo;
    adminInfo.valid = true;
    adminInfo.role  = "admin";

    EXPECT_CALL(*mgr_, decodeToken("valid.admin.token"))
        .WillOnce(Return(adminInfo));

    // Aucun champ rempli hormis le token
    securecloud::auth::RegisterRequest req;
    req.set_admin_token("valid.admin.token");

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(AuthServiceTest, RegisterUser_CustomRole) {
    TokenInfo adminInfo;
    adminInfo.valid = true;
    adminInfo.role  = "admin";

    EXPECT_CALL(*mgr_, decodeToken("valid.admin.token"))
        .WillOnce(Return(adminInfo));
    EXPECT_CALL(*db_, userExists("mod@example.com"))
        .WillOnce(Return(false));
    EXPECT_CALL(*db_, registerUserWithRole("Mod", "mod@example.com", _, "moderator"))
        .Times(1);

    securecloud::auth::RegisterRequest req;
    req.set_full_name("Mod");
    req.set_email("mod@example.com");
    req.set_password("pass");
    req.set_admin_token("valid.admin.token");
    req.set_role_name("moderator");

    securecloud::auth::RegisterResponse resp;
    auto status = svc_->RegisterUser(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
}

// ===========================================================================
// VALIDATE TOKEN
// ===========================================================================

TEST_F(AuthServiceTest, ValidateToken_Valid) {
    TokenInfo info;
    info.valid       = true;
    info.userId      = "42";
    info.email       = "alice@example.com";
    info.role        = "user";
    info.permissions = {"user.read"};

    EXPECT_CALL(*mgr_, decodeToken("good.token"))
        .WillOnce(Return(info));

    securecloud::auth::ValidateTokenRequest req;
    req.set_access_token("good.token");

    securecloud::auth::ValidateTokenResponse resp;
    auto status = svc_->ValidateToken(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(resp.valid());
    EXPECT_EQ(resp.user_id(), "42");
    EXPECT_EQ(resp.email(), "alice@example.com");
    EXPECT_EQ(resp.role(), "user");
    EXPECT_EQ(resp.permissions_size(), 1);
    EXPECT_EQ(resp.permissions(0), "user.read");
}

TEST_F(AuthServiceTest, ValidateToken_Invalid) {
    TokenInfo info;
    info.valid = false;

    EXPECT_CALL(*mgr_, decodeToken("expired.token"))
        .WillOnce(Return(info));

    securecloud::auth::ValidateTokenRequest req;
    req.set_access_token("expired.token");

    securecloud::auth::ValidateTokenResponse resp;
    auto status = svc_->ValidateToken(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());       // ValidateToken retourne OK même si le token est invalide
    EXPECT_FALSE(resp.valid());     // mais le champ valid = false
    EXPECT_TRUE(resp.user_id().empty());
}

TEST_F(AuthServiceTest, ValidateToken_EmptyToken) {
    TokenInfo info;
    info.valid = false;

    EXPECT_CALL(*mgr_, decodeToken(""))
        .WillOnce(Return(info));

    securecloud::auth::ValidateTokenRequest req;
    req.set_access_token("");

    securecloud::auth::ValidateTokenResponse resp;
    auto status = svc_->ValidateToken(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_FALSE(resp.valid());
}

// ===========================================================================
// LIST USERS
// ===========================================================================

TEST_F(AuthServiceTest, ListUsers_Success) {
    TokenInfo info;
    info.valid  = true;
    info.userId = "1";
    info.role   = "admin";

    EXPECT_CALL(*mgr_, decodeToken("admin.token"))
        .WillOnce(Return(info));

    std::vector<UserRecord> users = {
        makeUser(1, "alice@example.com", "", "admin"),
        makeUser(2, "bob@example.com",   "", "user"),
    };
    EXPECT_CALL(*db_, listUsers())
        .WillOnce(Return(users));

    securecloud::auth::ListUsersRequest req;
    req.set_access_token("admin.token");
    req.set_include_self(true);

    securecloud::auth::ListUsersResponse resp;
    auto status = svc_->ListUsers(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.users_size(), 2);
    EXPECT_EQ(resp.users(0).email(), "alice@example.com");
    EXPECT_EQ(resp.users(1).email(), "bob@example.com");
}

TEST_F(AuthServiceTest, ListUsers_ExcludeSelf) {
    TokenInfo info;
    info.valid  = true;
    info.userId = "1";   // alice est l'utilisateur connecté
    info.role   = "user";

    EXPECT_CALL(*mgr_, decodeToken("user.token"))
        .WillOnce(Return(info));

    std::vector<UserRecord> users = {
        makeUser(1, "alice@example.com", "", "user"),
        makeUser(2, "bob@example.com",   "", "user"),
    };
    EXPECT_CALL(*db_, listUsers())
        .WillOnce(Return(users));

    securecloud::auth::ListUsersRequest req;
    req.set_access_token("user.token");
    req.set_include_self(false);   // exclure soi-même

    securecloud::auth::ListUsersResponse resp;
    auto status = svc_->ListUsers(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.users_size(), 1);  // seulement bob
    EXPECT_EQ(resp.users(0).email(), "bob@example.com");
}

TEST_F(AuthServiceTest, ListUsers_InvalidToken) {
    TokenInfo info;
    info.valid = false;

    EXPECT_CALL(*mgr_, decodeToken("bad.token"))
        .WillOnce(Return(info));

    // listUsers NE doit PAS être appelé
    EXPECT_CALL(*db_, listUsers()).Times(0);

    securecloud::auth::ListUsersRequest req;
    req.set_access_token("bad.token");

    securecloud::auth::ListUsersResponse resp;
    auto status = svc_->ListUsers(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::UNAUTHENTICATED);
}

// ---------------------------------------------------------------------------
// Point d'entrée
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
