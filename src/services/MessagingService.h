#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QUrlQuery>
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
        const QString bodyContent = content.trimmed();
        if (conversationId.trimmed().isEmpty() || bodyContent.isEmpty()) {
            return;
        }

        const QNetworkRequest req = makeRequest(QStringLiteral("/conversations/%1/messages").arg(conversationId));
        const QJsonObject payload{{"content", bodyContent}};
        QNetworkReply* reply = m_network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, conversationId, bodyContent]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                reply->deleteLater();
                emit errorOccurred(QStringLiteral("Erreur envoi (%1): %2").arg(httpStatus).arg(formatNetworkError(reply, raw)));
                return;
            }

            reply->deleteLater();
            emit messageSent(conversationId, bodyContent);
            refreshMessages(conversationId);
            refreshContacts();
        });
    }

    void setGatewayBaseUrl(const QString& baseUrl) {
        m_gatewayBaseUrl = baseUrl.trimmed();
        if (m_gatewayBaseUrl.endsWith('/')) m_gatewayBaseUrl.chop(1);
    }
    QString gatewayBaseUrl() const { return m_gatewayBaseUrl; }

public slots:
    void refreshContacts() {
        const QNetworkRequest req = makeRequest(QStringLiteral("/users"));
        QNetworkReply* reply = m_network.get(req);
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                reply->deleteLater();
                emit errorOccurred(message);
            };

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

                contacts.push_back(Contact(userId, name, role.isEmpty() ? QStringLiteral("—") : role));
            }

            m_contacts = contacts;
            reply->deleteLater();
            emit contactsUpdated();
        });
    }

    void refreshMessages(const QString& conversationId, int limit = 200) {
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
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, cid]() {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                reply->deleteLater();
                emit errorOccurred(message);
            };

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
                const qint64 ts = static_cast<qint64>(obj.value("timestampUnix").toDouble(0));

                Message::Type type = Message::Type::Received;
                if (!m_currentUserId.isEmpty() && !senderId.isEmpty() && senderId == m_currentUserId) {
                    type = Message::Type::Sent;
                }
                out.push_back(Message(content, type, QDateTime::fromSecsSinceEpoch(ts > 0 ? ts : QDateTime::currentSecsSinceEpoch())));
            }

            m_messagesByConversation.insert(cid, out);
            reply->deleteLater();
            emit messagesUpdated(cid);
        });
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
            const QByteArray raw = reply->readAll();

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
    bool m_meFetchInFlight = false;
    QString m_pendingConversationRefresh;
};
