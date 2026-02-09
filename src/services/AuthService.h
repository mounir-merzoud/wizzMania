#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include <QFutureWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPromise>
#include <QUrl>
#include <QUrlQuery>
#include "../models/User.h"

/**
 * @brief Service d'authentification
 * 
 * Version "propre": le front appelle un Gateway HTTP, qui proxy vers gRPC auth_service.
 */
class AuthService : public QObject {
    Q_OBJECT
    
public:
    static AuthService& instance() {
        static AuthService instance;
        return instance;
    }
    
    /**
     * @brief Authentifie un utilisateur
     * @param username Nom d'utilisateur
     * @param password Mot de passe
     * @return Future contenant l'utilisateur si succès, vide sinon
     */
    QFuture<User> login(const QString& username, const QString& password) {
        QPromise<User> promise;
        auto future = promise.future();

        m_lastError.clear();

        const QString trimmedUsername = username.trimmed();
        if (trimmedUsername.isEmpty() || password.isEmpty()) {
            m_lastError = QStringLiteral("Champs manquants");
            emit authenticationFailed(m_lastError);
            promise.addResult(User());
            promise.finish();
            return future;
        }

        const QUrl url(m_gatewayBaseUrl + QStringLiteral("/login"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        const QJsonObject payload{{"username", trimmedUsername}, {"password", password}};
        const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkReply* reply = m_network.post(req, body);
        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, reply, promise = std::move(promise), trimmedUsername]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto fail = [&](const QString& message) {
                m_lastError = message;
                emit authenticationFailed(message);
                promise.addResult(User());
                promise.finish();
            };

            if (reply->error() != QNetworkReply::NoError) {
                const QString detail = QString::fromUtf8(raw).trimmed();
                const QString msg = detail.isEmpty()
                                        ? reply->errorString()
                                        : QStringLiteral("%1").arg(detail);
                reply->deleteLater();
                fail(QStringLiteral("Erreur réseau (%1): %2").arg(httpStatus).arg(msg));
                return;
            }

            QJsonParseError jsonErr{};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &jsonErr);
            if (jsonErr.error != QJsonParseError::NoError || !doc.isObject()) {
                reply->deleteLater();
                fail(QStringLiteral("Réponse gateway invalide"));
                return;
            }

            const QJsonObject obj = doc.object();
            const QString accessToken = obj.value("accessToken").toString(obj.value("access_token").toString());
            const QString refreshToken = obj.value("refreshToken").toString(obj.value("refresh_token").toString());
            const qint64 expiresIn = static_cast<qint64>(obj.value("expiresIn").toDouble(obj.value("expires_in").toDouble(0)));

            if (accessToken.isEmpty()) {
                const QString message = obj.value("message").toString(QStringLiteral("Identifiants invalides"));
                reply->deleteLater();
                fail(message);
                return;
            }

            m_accessToken = accessToken;
            m_refreshToken = refreshToken;
            m_expiresIn = expiresIn;

            // Le LoginResponse ne renvoie pas forcément user_id/email; on garde un user minimal.
            m_currentUser = User(QStringLiteral("unknown"), trimmedUsername, trimmedUsername);
            reply->deleteLater();

            emit userLoggedIn(m_currentUser);
            promise.addResult(m_currentUser);
            promise.finish();
        });

        return future;
    }
    
    /**
     * @brief Déconnecte l'utilisateur actuel
     */
    void logout() {
        m_currentUser = User();
        m_accessToken.clear();
        m_refreshToken.clear();
        m_expiresIn = 0;
        m_lastError.clear();
        emit userLoggedOut();
    }
    
    /**
     * @brief Inscrit un nouvel utilisateur
     */
    QFuture<User> registerUser(const QString& username, const QString& email, const QString& password) {
        // TODO: Ajouter endpoint gateway /register -> auth_service/RegisterUser
        Q_UNUSED(password);
        
        User user("new", username, email);
        QPromise<User> promise;
        promise.addResult(user);
        promise.finish();
        return promise.future();
    }
    
    User currentUser() const { return m_currentUser; }
    bool isLoggedIn() const { return !m_currentUser.id().isEmpty(); }

    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }
    qint64 expiresIn() const { return m_expiresIn; }
    QString lastError() const { return m_lastError; }

    void setGatewayBaseUrl(const QString& baseUrl) {
        m_gatewayBaseUrl = baseUrl.trimmed();
        if (m_gatewayBaseUrl.endsWith('/')) m_gatewayBaseUrl.chop(1);
    }
    QString gatewayBaseUrl() const { return m_gatewayBaseUrl; }
    
signals:
    void userLoggedIn(const User& user);
    void userLoggedOut();
    void authenticationFailed(const QString& error);
    
private:
    AuthService() = default;
    ~AuthService() = default;
    AuthService(const AuthService&) = delete;
    AuthService& operator=(const AuthService&) = delete;
    
    User m_currentUser;

    QNetworkAccessManager m_network;
    QString m_gatewayBaseUrl = QStringLiteral("http://localhost:8080");
    QString m_accessToken;
    QString m_refreshToken;
    qint64 m_expiresIn = 0;
    QString m_lastError;
};
