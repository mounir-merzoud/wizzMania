#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QFuture>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPromise>
#include <QStringList>
#include <QUrl>
#include <QUrlQuery>
#include <functional>
#include <memory>
#include "../models/Contact.h"
#include "../models/Message.h"
#include "AuthService.h"

/**
 * @brief Service de messagerie
 *
 * Version "propre": le front appelle le Gateway HTTP, qui proxy vers gRPC messaging_service.
 * L'UI reste synchrone (getContacts/getMessages) mais les données sont mises en cache et
 * alimentées via refreshContacts/refreshMessages.
 */
class MessagingService : public QObject {
    Q_OBJECT
    
public:
    static MessagingService& instance() {
        static MessagingService instance;
        return instance;
    }
    
    /**
     * @brief Récupère la liste des contacts
     */
    QList<Contact> getContacts() const {
        return m_contacts;
    }
    
    /**
     * @brief Récupère les messages d'une conversation
     */
    QList<Message> getMessages(const QString& conversationId) const {
        return m_messagesByConversation.value(conversationId);
    }
    
    /**
     * @brief Envoie un message
     */
    void sendMessage(const QString& conversationId, const QString& content) {
        sendMessageImpl(conversationId, content, /*hasRetried*/ false);
    }

    void setGatewayBaseUrl(const QString& baseUrl) {
        m_gatewayBaseUrl = baseUrl.trimmed();
        if (m_gatewayBaseUrl.endsWith('/')) m_gatewayBaseUrl.chop(1);
    }
    QString gatewayBaseUrl() const { return m_gatewayBaseUrl; }

    QString lastError() const { return m_lastError; }

    /**
     * @brief Crée un salon (room) via le gateway (admin seulement)
     * @return Future<bool> true si création OK
     */
    QFuture<bool> createRoomAsAdmin(const QString& title, const QStringList& participantEmails) {
        auto promise = std::make_shared<QPromise<bool>>();
        auto future = promise->future();
        createRoomAsAdminImpl(title, participantEmails, /*hasRetried*/ false, promise);
        return future;
    }

    /**
     * @brief Supprime un salon via le gateway (admin seulement)
     * @return Future<bool> true si suppression OK
     */
    QFuture<bool> deleteRoomAsAdmin(const QString& roomId) {
        auto promise = std::make_shared<QPromise<bool>>();
        auto future = promise->future();
        deleteRoomAsAdminImpl(roomId, /*hasRetried*/ false, promise);
        return future;
    }

public slots:
    void refreshContacts() {
        refreshContactsImpl(/*hasRetried*/ false);
    }

    void refreshMessages(const QString& conversationId, int limit = 200) {
        refreshMessagesImpl(conversationId, limit, /*hasRetried*/ false);
    }
    
signals:
    void contactsUpdated();
    void messageSent(const QString& contactId, const QString& content);
    void messageReceived(const QString& contactId, const Message& message);
    void contactStatusChanged(const QString& contactId, const QString& status);
    void messagesUpdated(const QString& conversationId);
    void errorOccurred(const QString& error);
    
private:
    MessagingService() {
        connect(&AuthService::instance(), &AuthService::userLoggedIn, this, [this](const User&) {
            ensureMe();
            refreshContacts();
        });
        connect(&AuthService::instance(), &AuthService::userLoggedOut, this, [this]() {
            m_contacts.clear();
            m_messagesByConversation.clear();
            m_currentUserId.clear();
            emit contactsUpdated();
        });
    }
    ~MessagingService() = default;
    MessagingService(const MessagingService&) = delete;
    MessagingService& operator=(const MessagingService&) = delete;

    QNetworkRequest makeRequest(const QString& path) const {
        const QUrl url(m_gatewayBaseUrl + path);
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QString token = AuthService::instance().accessToken();
        if (!token.isEmpty()) {
            req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
        }
        return req;
    }

    void refreshAccessTokenThen(const std::function<void(bool)>& cont) {
        auto* watcher = new QFutureWatcher<bool>(this);
        QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [watcher, cont]() {
            const bool ok = watcher->future().result();
            watcher->deleteLater();
            cont(ok);
        });
        watcher->setFuture(AuthService::instance().refreshAccessToken());
    }

    QString sessionExpiredMessage() const {
        const QString detail = AuthService::instance().lastError().trimmed();
        if (!detail.isEmpty()) {
            return QStringLiteral("Session expirée: %1").arg(detail);
        }
        return QStringLiteral("Session expirée");
    }

    void sendMessageImpl(const QString& conversationId, const QString& content, bool hasRetried) {
        const QString cid = conversationId.trimmed();
        const QString bodyContent = content.trimmed();
        if (cid.isEmpty() || bodyContent.isEmpty()) {
            return;
        }

        const QNetworkRequest req = makeRequest(QStringLiteral("/conversations/%1/messages").arg(cid));
        const QJsonObject payload{{"content", bodyContent}};
        QNetworkReply* reply = m_network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cid, bodyContent, hasRetried]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                refreshAccessTokenThen([this, cid, bodyContent](bool ok) {
                    if (!ok) {
                        emit errorOccurred(sessionExpiredMessage());
                        return;
                    }
                    sendMessageImpl(cid, bodyContent, /*hasRetried*/ true);
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                reply->deleteLater();
                emit errorOccurred(QStringLiteral("Erreur envoi (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            reply->deleteLater();
            emit messageSent(cid, bodyContent);
            refreshMessages(cid);
            refreshContacts();
        });
    }

    void refreshContactsImpl(bool hasRetried) {
        const QNetworkRequest req = makeRequest(QStringLiteral("/users"));
        QNetworkReply* reply = m_network.get(req);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, hasRetried]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                reply->deleteLater();
                emit errorOccurred(message);
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                refreshAccessTokenThen([this](bool ok) {
                    if (!ok) {
                        emit errorOccurred(sessionExpiredMessage());
                        return;
                    }
                    refreshContactsImpl(/*hasRetried*/ true);
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                fail(QStringLiteral("Erreur contacts (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            QJsonObject obj;
            QString parseError;
            if (!parseJsonObject(raw, &obj, &parseError)) {
                fail(parseError.isEmpty() ? QStringLiteral("Réponse users invalide") : parseError);
                return;
            }

            const QJsonArray users = obj.value("users").toArray();
            QList<Contact> contacts;
            contacts.reserve(users.size());
            for (const auto& item : users) {
                const QJsonObject obj = item.toObject();
                const QString userId = obj.value("userId").toString(obj.value("user_id").toString());
                if (userId.isEmpty()) continue;

                const QString fullName = obj.value("fullName").toString(obj.value("full_name").toString());
                const QString email = obj.value("email").toString();
                const QString role = obj.value("role").toString();
                const QString name = !fullName.isEmpty() ? fullName : (!email.isEmpty() ? email : userId);

                contacts.push_back(Contact(userId, name, role.isEmpty() ? QStringLiteral("—") : role, email));
            }

            // Fetch rooms (non-fatal if it fails; users list remains available)
            const QNetworkRequest roomsReq = makeRequest(QStringLiteral("/rooms"));
            QNetworkReply* roomsReply = m_network.get(roomsReq);
            QObject::connect(roomsReply, &QNetworkReply::finished, this, [this, roomsReply, contacts]() mutable {
                const int roomsHttp = roomsReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
                const QByteArray roomsRaw = roomsReply->readAll();

                auto isHttpOk2 = [&](int status) {
                    return status >= 200 && status < 300;
                };

                if (roomsReply->error() == QNetworkReply::NoError && isHttpOk2(roomsHttp)) {
                    QJsonObject roomsObj;
                    QString parseError;
                    if (parseJsonObject(roomsRaw, &roomsObj, &parseError)) {
                        const QJsonArray rooms = roomsObj.value("rooms").toArray();
                        contacts.reserve(contacts.size() + rooms.size());
                        for (const auto& item : rooms) {
                            const QJsonObject r = item.toObject();
                            const QString roomId = r.value("roomId").toString(r.value("room_id").toString());
                            if (roomId.isEmpty()) continue;
                            const QString title = r.value("title").toString();
                            contacts.push_back(Contact(roomId, title.isEmpty() ? roomId : title, QStringLiteral("Room")));
                        }
                    }
                }

                m_contacts = contacts;
                roomsReply->deleteLater();
                emit contactsUpdated();
            });

            reply->deleteLater();
        });
    }

    void refreshMessagesImpl(const QString& conversationId, int limit, bool hasRetried) {
        const QString cid = conversationId.trimmed();
        if (cid.isEmpty()) {
            return;
        }

        ensureMe(cid);

        QUrl url(m_gatewayBaseUrl + QStringLiteral("/conversations/%1/messages").arg(cid));
        QUrlQuery q;
        q.addQueryItem(QStringLiteral("limit"), QString::number(limit));
        url.setQuery(q);

        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        const QString token = AuthService::instance().accessToken();
        if (!token.isEmpty()) {
            req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());
        }

        QNetworkReply* reply = m_network.get(req);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cid, limit, hasRetried]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                reply->deleteLater();
                emit errorOccurred(message);
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                refreshAccessTokenThen([this, cid, limit](bool ok) {
                    if (!ok) {
                        emit errorOccurred(sessionExpiredMessage());
                        return;
                    }
                    refreshMessagesImpl(cid, limit, /*hasRetried*/ true);
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                fail(QStringLiteral("Erreur messages (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            QJsonObject obj;
            QString parseError;
            if (!parseJsonObject(raw, &obj, &parseError)) {
                fail(parseError.isEmpty() ? QStringLiteral("Réponse messages invalide") : parseError);
                return;
            }

            const QJsonArray messages = obj.value("messages").toArray();
            QList<Message> out;
            out.reserve(messages.size());
            for (const auto& item : messages) {
                const QJsonObject obj = item.toObject();
                const QString content = obj.value("content").toString();
                const QString senderId = obj.value("senderId").toString();
                const QString messageId = obj.value("messageId").toString();
                const qint64 ts = static_cast<qint64>(obj.value("timestampUnix").toDouble(0));

                Message::Type type = Message::Type::Received;
                if (!m_currentUserId.isEmpty() && !senderId.isEmpty() && senderId == m_currentUserId) {
                    type = Message::Type::Sent;
                }
                out.push_back(Message(
                    content,
                    type,
                    QDateTime::fromSecsSinceEpoch(ts > 0 ? ts : QDateTime::currentSecsSinceEpoch()),
                    senderId,
                    messageId
                ));
            }

            m_messagesByConversation.insert(cid, out);
            reply->deleteLater();
            emit messagesUpdated(cid);
        });
    }

    void createRoomAsAdminImpl(const QString& title,
                              const QStringList& participantEmails,
                              bool hasRetried,
                              const std::shared_ptr<QPromise<bool>>& promise) {
        m_lastError.clear();

        if (!AuthService::instance().isAdmin()) {
            m_lastError = QStringLiteral("Accès refusé (admin requis)");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString token = AuthService::instance().accessToken().trimmed();
        if (token.isEmpty()) {
            m_lastError = QStringLiteral("Non authentifié");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString roomTitle = title.trimmed();
        if (roomTitle.isEmpty()) {
            m_lastError = QStringLiteral("Titre requis");
            promise->addResult(false);
            promise->finish();
            return;
        }

        QJsonArray emails;
        for (const auto& e : participantEmails) {
            const QString mail = e.trimmed();
            if (!mail.isEmpty()) {
                emails.push_back(mail);
            }
        }

        const QNetworkRequest req = makeRequest(QStringLiteral("/rooms"));
        const QJsonObject payload{{"title", roomTitle}, {"participantEmails", emails}};
        QNetworkReply* reply = m_network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, title, participantEmails, hasRetried, promise]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                reply->deleteLater();
                promise->addResult(false);
                promise->finish();
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                refreshAccessTokenThen([this, title, participantEmails, promise](bool ok) {
                    if (!ok) {
                        m_lastError = sessionExpiredMessage();
                        promise->addResult(false);
                        promise->finish();
                        return;
                    }
                    createRoomAsAdminImpl(title, participantEmails, /*hasRetried*/ true, promise);
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                fail(QStringLiteral("Erreur création salon (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            QJsonObject obj;
            QString parseError;
            if (!parseJsonObject(raw, &obj, &parseError)) {
                fail(parseError.isEmpty() ? QStringLiteral("Réponse rooms invalide") : parseError);
                return;
            }

            const bool success = obj.value("success").toBool(false);
            const QString message = obj.value("message").toString();
            if (!success) {
                fail(message.isEmpty() ? QStringLiteral("Création salon échouée") : message);
                return;
            }

            reply->deleteLater();
            promise->addResult(true);
            promise->finish();
        });
    }

    void deleteRoomAsAdminImpl(const QString& roomId,
                               bool hasRetried,
                               const std::shared_ptr<QPromise<bool>>& promise) {
        m_lastError.clear();

        if (!AuthService::instance().isAdmin()) {
            m_lastError = QStringLiteral("Accès refusé (admin requis)");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString token = AuthService::instance().accessToken().trimmed();
        if (token.isEmpty()) {
            m_lastError = QStringLiteral("Non authentifié");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString rid = roomId.trimmed();
        if (rid.isEmpty() || !rid.startsWith(QStringLiteral("room:"))) {
            m_lastError = QStringLiteral("room_id invalide");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QNetworkRequest req = makeRequest(QStringLiteral("/rooms/%1").arg(rid));
        QNetworkReply* reply = m_network.deleteResource(req);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, rid, hasRetried, promise]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                reply->deleteLater();
                promise->addResult(false);
                promise->finish();
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                refreshAccessTokenThen([this, rid, promise](bool ok) {
                    if (!ok) {
                        m_lastError = sessionExpiredMessage();
                        promise->addResult(false);
                        promise->finish();
                        return;
                    }
                    deleteRoomAsAdminImpl(rid, /*hasRetried*/ true, promise);
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                fail(QStringLiteral("Erreur suppression salon (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            QJsonObject obj;
            QString parseError;
            if (!parseJsonObject(raw, &obj, &parseError)) {
                fail(parseError.isEmpty() ? QStringLiteral("Réponse delete room invalide") : parseError);
                return;
            }

            const bool success = obj.value("success").toBool(false);
            const QString message = obj.value("message").toString();
            reply->deleteLater();

            if (!success) {
                m_lastError = message.isEmpty() ? QStringLiteral("Suppression salon échouée") : message;
                promise->addResult(false);
                promise->finish();
                return;
            }

            promise->addResult(true);
            promise->finish();
        });
    }

    void ensureMe(const QString& refreshConversationAfter = QString()) {
        if (!refreshConversationAfter.isEmpty()) {
            m_pendingConversationRefresh = refreshConversationAfter;
        }

        const QString token = AuthService::instance().accessToken();
        if (token.isEmpty() || !m_currentUserId.isEmpty() || m_meFetchInFlight) {
            return;
        }
        m_meFetchInFlight = true;

        const QNetworkRequest req = makeRequest(QStringLiteral("/me"));
        QNetworkReply* reply = m_network.get(req);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            m_meFetchInFlight = false;
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            if (httpStatus == 401) {
                reply->deleteLater();
                // Best-effort refresh. If it fails, next public calls will surface the error.
                refreshAccessTokenThen([this](bool ok) {
                    if (!ok) {
                        emit errorOccurred(sessionExpiredMessage());
                        return;
                    }
                    ensureMe();
                });
                return;
            }

            if (reply->error() != QNetworkReply::NoError) {
                reply->deleteLater();
                return;
            }

            QJsonParseError jsonErr{};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &jsonErr);
            if (jsonErr.error != QJsonParseError::NoError || !doc.isObject()) {
                reply->deleteLater();
                return;
            }

            const QJsonObject obj = doc.object();
            const QString userId = obj.value("userId").toString(obj.value("user_id").toString());
            if (!userId.isEmpty()) {
                m_currentUserId = userId;
            }
            reply->deleteLater();

            if (!m_pendingConversationRefresh.isEmpty()) {
                const QString cid = m_pendingConversationRefresh;
                m_pendingConversationRefresh.clear();
                refreshMessages(cid);
            }
        });
    }

    static QString formatNetworkError(const QNetworkReply* reply, const QByteArray& raw) {
        const QString detail = QString::fromUtf8(raw).trimmed();
        if (!detail.isEmpty()) {
            return detail;
        }
        return reply ? reply->errorString() : QStringLiteral("Erreur réseau");
    }

    static bool parseJsonObject(const QByteArray& raw, QJsonObject* out, QString* error) {
        if (!out) {
            if (error) *error = QStringLiteral("Réponse JSON invalide");
            return false;
        }
        QJsonParseError jsonErr{};
        const QJsonDocument doc = QJsonDocument::fromJson(raw, &jsonErr);
        if (jsonErr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (error) *error = QStringLiteral("Réponse JSON invalide");
            return false;
        }
        *out = doc.object();
        return true;
    }

    QNetworkAccessManager m_network;
    QString m_gatewayBaseUrl = QStringLiteral("http://localhost:8080");
    QList<Contact> m_contacts;
    QHash<QString, QList<Message>> m_messagesByConversation;
    QString m_currentUserId;
    QString m_lastError;
    bool m_meFetchInFlight = false;
    QString m_pendingConversationRefresh;
};
