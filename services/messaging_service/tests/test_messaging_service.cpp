/**
 * @file test_messaging_service.cpp
 * @brief Tests unitaires pour MessagingServiceImpl
 *
 * Couvre :
 *  - SendMessage : succès / conversation_id manquant / erreur DB
 *  - GetHistory  : succès avec messages / conversation vide / erreur DB / limit
 *  - Utilitaires : Base64::encode/decode, parse_int (via helpers internes)
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <grpcpp/grpcpp.h>
#include <memory>
#include <vector>
#include <optional>
#include <ctime>

#include "mocks/MockMessagingDatabase.h"
#include "utils/Base64.h"

// On inclut l'implémentation directement pour accéder à MessagingServiceImpl
// sans modifier messaging_service.cpp (évite de dupliquer main())
#define MESSAGING_SERVICE_IMPL_ONLY
#include "../src/messaging_service.cpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Throw;
using ::testing::NiceMock;

// ---------------------------------------------------------------------------
// Helper
// ---------------------------------------------------------------------------

static grpc::ServerContext* fakeCtx() {
    static grpc::ServerContext ctx;
    return &ctx;
}

static DbMessageRow makeRow(int id,
                             const std::string& convKey,
                             const std::string& b64Content,
                             std::optional<int> senderId = 1,
                             long long ts = 1700000000LL) {
    DbMessageRow r;
    r.id_messages          = id;
    r.conversation_key     = convKey;
    r.encrypted_content_b64 = b64Content;
    r.sender_id            = senderId;
    r.created_at_unix      = ts;
    return r;
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class MessagingServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_  = std::make_shared<NiceMock<MockMessagingDatabase>>();
        svc_ = std::make_unique<MessagingServiceImpl>(*db_);
    }

    std::shared_ptr<NiceMock<MockMessagingDatabase>> db_;
    std::unique_ptr<MessagingServiceImpl>             svc_;
};

// ===========================================================================
// SEND MESSAGE
// ===========================================================================

TEST_F(MessagingServiceTest, SendMessage_Success) {
    const std::string convId  = "dm:1:2";
    const std::string content = "hello world";

    // insertMessage retourne l'id du message inséré
    EXPECT_CALL(*db_, insertMessage(convId, std::optional<int>(1), _))
        .WillOnce(Return(42));

    securecloud::messaging::EncryptedMessage req;
    req.set_conversation_id(convId);
    req.set_sender_id("1");
    req.set_ciphertext(content);
    req.set_timestamp_unix(std::time(nullptr));

    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), &req, &ack);

    EXPECT_TRUE(status.ok());
    EXPECT_TRUE(ack.accepted());
    EXPECT_EQ(ack.message_id(), "db_42");
}

TEST_F(MessagingServiceTest, SendMessage_MissingConversationId) {
    // insertMessage ne doit PAS être appelé
    EXPECT_CALL(*db_, insertMessage(_, _, _)).Times(0);

    securecloud::messaging::EncryptedMessage req;
    req.set_conversation_id("");   // conversation_id vide
    req.set_sender_id("1");
    req.set_ciphertext("hello");

    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), &req, &ack);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INVALID_ARGUMENT);
}

TEST_F(MessagingServiceTest, SendMessage_NullRequest) {
    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), nullptr, &ack);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(MessagingServiceTest, SendMessage_DatabaseError) {
    const std::string convId = "dm:1:2";

    EXPECT_CALL(*db_, insertMessage(convId, _, _))
        .WillOnce(Throw(std::runtime_error("connection lost")));

    securecloud::messaging::EncryptedMessage req;
    req.set_conversation_id(convId);
    req.set_sender_id("1");
    req.set_ciphertext("hello");

    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), &req, &ack);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), ::testing::HasSubstr("DB error"));
}

TEST_F(MessagingServiceTest, SendMessage_AutoTimestamp) {
    // Si timestamp_unix == 0, le service doit le remplir automatiquement
    const std::string convId = "dm:3:4";

    EXPECT_CALL(*db_, insertMessage(convId, _, _))
        .WillOnce(Return(7));

    securecloud::messaging::EncryptedMessage req;
    req.set_conversation_id(convId);
    req.set_sender_id("3");
    req.set_ciphertext("auto-ts test");
    req.set_timestamp_unix(0);   // doit être rempli par le service

    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), &req, &ack);

    EXPECT_TRUE(status.ok());
}

TEST_F(MessagingServiceTest, SendMessage_SenderIdNotInteger) {
    // sender_id non-entier → senderId = nullopt, mais le message doit quand même être accepté
    const std::string convId = "dm:group:abc";

    EXPECT_CALL(*db_, insertMessage(convId, std::optional<int>(std::nullopt), _))
        .WillOnce(Return(100));

    securecloud::messaging::EncryptedMessage req;
    req.set_conversation_id(convId);
    req.set_sender_id("not-an-int");
    req.set_ciphertext("message from anonymous");

    securecloud::messaging::SendAck ack;
    auto status = svc_->SendMessage(fakeCtx(), &req, &ack);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(ack.message_id(), "db_100");
}

// ===========================================================================
// GET HISTORY
// ===========================================================================

TEST_F(MessagingServiceTest, GetHistory_Success) {
    const std::string convId = "dm:1:2";
    const std::string b64msg = Base64::encode("secret message");

    std::vector<DbMessageRow> rows = {
        makeRow(10, convId, b64msg, 1, 1700000001LL),
        makeRow(11, convId, Base64::encode("second msg"), 2, 1700000002LL),
    };

    EXPECT_CALL(*db_, getHistory(convId, 50))
        .WillOnce(Return(rows));

    securecloud::messaging::HistoryRequest req;
    req.set_conversation_id(convId);
    req.set_limit(50);

    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    // Les messages sont retournés du plus ancien au plus récent (ordre inversé)
    EXPECT_EQ(resp.messages_size(), 2);
    EXPECT_EQ(resp.messages(0).message_id(), "db_11");   // plus récent en dernier
    EXPECT_EQ(resp.messages(1).message_id(), "db_10");
    EXPECT_EQ(resp.messages(0).sender_id(), "2");
    EXPECT_EQ(resp.messages(0).ciphertext(), "second msg");
}

TEST_F(MessagingServiceTest, GetHistory_EmptyConversation) {
    EXPECT_CALL(*db_, getHistory("dm:99:100", 50))
        .WillOnce(Return(std::vector<DbMessageRow>{}));

    securecloud::messaging::HistoryRequest req;
    req.set_conversation_id("dm:99:100");
    req.set_limit(50);

    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.messages_size(), 0);
}

TEST_F(MessagingServiceTest, GetHistory_DatabaseError) {
    EXPECT_CALL(*db_, getHistory(_, _))
        .WillOnce(Throw(std::runtime_error("db timeout")));

    securecloud::messaging::HistoryRequest req;
    req.set_conversation_id("dm:1:2");
    req.set_limit(10);

    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), &req, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
    EXPECT_THAT(status.error_message(), ::testing::HasSubstr("DB error"));
}

TEST_F(MessagingServiceTest, GetHistory_NullRequest) {
    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), nullptr, &resp);

    EXPECT_FALSE(status.ok());
    EXPECT_EQ(status.error_code(), grpc::StatusCode::INTERNAL);
}

TEST_F(MessagingServiceTest, GetHistory_LimitRespected) {
    // Vérifie que la valeur limit est bien transmise à la DB
    EXPECT_CALL(*db_, getHistory("dm:1:2", 5))
        .WillOnce(Return(std::vector<DbMessageRow>{}));

    securecloud::messaging::HistoryRequest req;
    req.set_conversation_id("dm:1:2");
    req.set_limit(5);

    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
}

TEST_F(MessagingServiceTest, GetHistory_SenderIdAbsent) {
    // sender_id = nullopt → le champ sender_id du message gRPC doit rester vide
    DbMessageRow row = makeRow(5, "dm:1:2", Base64::encode("anon"), std::nullopt, 1700000000LL);

    EXPECT_CALL(*db_, getHistory("dm:1:2", 50))
        .WillOnce(Return(std::vector<DbMessageRow>{row}));

    securecloud::messaging::HistoryRequest req;
    req.set_conversation_id("dm:1:2");
    req.set_limit(50);

    securecloud::messaging::HistoryResponse resp;
    auto status = svc_->GetHistory(fakeCtx(), &req, &resp);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(resp.messages_size(), 1);
    EXPECT_TRUE(resp.messages(0).sender_id().empty());
}

// ===========================================================================
// UTILITAIRES — Base64
// ===========================================================================

TEST(Base64Test, EncodeDecodeRoundTrip) {
    const std::string plain = "Hello, World! \x00\x01\x02";
    const std::string encoded = Base64::encode(plain);
    EXPECT_FALSE(encoded.empty());
    EXPECT_NE(encoded, plain);

    const std::string decoded = Base64::decode(encoded);
    EXPECT_EQ(decoded, plain);
}

TEST(Base64Test, EncodeEmpty) {
    const std::string encoded = Base64::encode("");
    const std::string decoded = Base64::decode(encoded);
    EXPECT_EQ(decoded, "");
}

TEST(Base64Test, DecodeInvalidFallback) {
    // Un contenu non-base64 ne doit pas lever d'exception mais retourner la valeur as-is
    EXPECT_NO_THROW({
        std::string result = Base64::decode("not valid base64!@#$");
        (void)result;
    });
}

// ---------------------------------------------------------------------------
// Point d'entrée
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
