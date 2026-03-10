#pragma once

#include <QObject>
#include <QString>
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
#include <memory>
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

        // Prevent out-of-order replies (e.g., double-click login) from overwriting tokens.
        const quint64 requestSeq = ++m_loginRequestSeq;

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
                         [this, reply, promise = std::move(promise), trimmedUsername, requestSeq]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                emit authenticationFailed(message);
                promise.addResult(User());
                promise.finish();
            };

            // Ignore stale responses if a newer login was started.
            if (requestSeq != m_loginRequestSeq) {
                reply->deleteLater();
                promise.addResult(User());
                promise.finish();
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
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
            const QString role = obj.value("role").toString();
            const QJsonValue permsValue = obj.value("permissions");

            if (accessToken.isEmpty()) {
                const QString message = obj.value("message").toString(QStringLiteral("Identifiants invalides"));
                reply->deleteLater();
                fail(message);
                return;
            }

            if (refreshToken.trimmed().isEmpty()) {
                // Refresh token is required for session continuity (auto-refresh on 401).
                // If it's missing, the running gateway/auth_service is likely outdated.
                reply->deleteLater();
                fail(QStringLiteral("Login OK mais refresh token manquant. Redémarre/rebuild gateway_service + auth_service."));
                return;
            }

            m_accessToken = accessToken;
            m_refreshToken = refreshToken;
            m_expiresIn = expiresIn;
            m_role = role;
            m_permissions.clear();
            if (permsValue.isArray()) {
                const QJsonArray arr = permsValue.toArray();
                for (const auto& v : arr) {
                    if (v.isString()) {
                        m_permissions.push_back(v.toString());
                    }
                }
            }

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
    void logout(bool clearError = true) {
        m_currentUser = User();
        m_accessToken.clear();
        m_refreshToken.clear();
        m_expiresIn = 0;
        m_role.clear();
        m_permissions.clear();
        if (clearError) {
            m_lastError.clear();
        }
        emit userLoggedOut();
    }

    /**
     * @brief Renouvelle l'access token via le refresh token (gateway /refresh)
     * @return Future<bool> true si refresh OK
     */
    QFuture<bool> refreshAccessToken() {
        QPromise<bool> promise;
        auto future = promise.future();

        m_lastError.clear();

        const QString rt = m_refreshToken.trimmed();
        if (rt.isEmpty()) {
            m_lastError = QStringLiteral("Refresh token manquant");
            promise.addResult(false);
            promise.finish();
            return future;
        }

        const QUrl url(m_gatewayBaseUrl + QStringLiteral("/refresh"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

        const QJsonObject payload{{"refreshToken", rt}};
        QNetworkReply* reply = m_network.post(req, QJsonDocument(payload).toJson(QJsonDocument::Compact));
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, promise = std::move(promise)]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                reply->deleteLater();
                promise.addResult(false);
                promise.finish();
            };

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                const QString detail = QString::fromUtf8(raw).trimmed();
                const QString msg = detail.isEmpty() ? reply->errorString() : detail;
                const QString errorMsg = QStringLiteral("Erreur refresh (%1): %2").arg(httpStatus).arg(msg);
                // If refresh is rejected by server, session is no longer recoverable.
                if (httpStatus == 401) {
                    m_lastError = errorMsg;
                    reply->deleteLater();
                    promise.addResult(false);
                    promise.finish();
                    logout(/*clearError*/ false);
                    return;
                }
                fail(errorMsg);
                return;
            }

            QJsonParseError jsonErr{};
            const QJsonDocument doc = QJsonDocument::fromJson(raw, &jsonErr);
            if (jsonErr.error != QJsonParseError::NoError || !doc.isObject()) {
                fail(QStringLiteral("Réponse refresh invalide"));
                return;
            }

            const QJsonObject obj = doc.object();
            const QString accessToken = obj.value("accessToken").toString(obj.value("access_token").toString());
            const qint64 expiresIn = static_cast<qint64>(obj.value("expiresIn").toDouble(obj.value("expires_in").toDouble(0)));
            if (accessToken.isEmpty()) {
                const QString message = obj.value("message").toString(QStringLiteral("Refresh échoué"));
                // Defensive: treat as auth failure.
                if (httpStatus == 401) {
                    m_lastError = message;
                    reply->deleteLater();
                    promise.addResult(false);
                    promise.finish();
                    logout(/*clearError*/ false);
                    return;
                }
                fail(message);
                return;
            }

            m_accessToken = accessToken;
            m_expiresIn = expiresIn;
            reply->deleteLater();
            promise.addResult(true);
            promise.finish();
        });

        return future;
    }

    /**
     * @brief Déconnexion serveur (révocation) via gateway /logout
     * @return Future<bool> true si révocation OK
     */
    QFuture<bool> logoutFromServer() {
        QPromise<bool> promise;
        auto future = promise.future();

        m_lastError.clear();

        const QString token = m_accessToken.trimmed();
        if (token.isEmpty()) {
            promise.addResult(false);
            promise.finish();
            return future;
        }

        const QUrl url(m_gatewayBaseUrl + QStringLiteral("/logout"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());

        QNetworkReply* reply = m_network.post(req, QByteArray());
        QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, promise = std::move(promise)]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                m_lastError = QStringLiteral("Erreur logout (%1): %2").arg(httpStatus).arg(QString::fromUtf8(raw).trimmed());
                reply->deleteLater();
                promise.addResult(false);
                promise.finish();
                return;
            }

            reply->deleteLater();
            promise.addResult(true);
            promise.finish();
        });

        return future;
    }

    /**
     * @brief Crée un utilisateur via le gateway (admin seulement)
     * @return Future<bool> true si création OK
     */
    QFuture<bool> createUserAsAdmin(const QString& fullName,
                                   const QString& email,
                                   const QString& password,
                                   const QString& roleName = QStringLiteral("user")) {
        auto promise = std::make_shared<QPromise<bool>>();
        auto future = promise->future();
        createUserAsAdminImpl(fullName, email, password, roleName, /*hasRetried*/ false, promise);
        return future;
    }

    /**
     * @brief Supprime un utilisateur via le gateway (admin seulement)
     * @return Future<bool> true si suppression OK
     */
    QFuture<bool> deleteUserAsAdmin(const QString& userId) {
        auto promise = std::make_shared<QPromise<bool>>();
        auto future = promise->future();
        deleteUserAsAdminImpl(userId, /*hasRetried*/ false, promise);
        return future;
    }
    
    /**
     * @brief Inscrit un nouvel utilisateur
     */
    QFuture<User> registerUser(const QString& username, const QString& email, const QString& password) {
        QPromise<User> promise;
        auto future = promise.future();

        auto createFuture = createUserAsAdmin(username, email, password, QStringLiteral("user"));
        auto* watcher = new QFutureWatcher<bool>(this);
        QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, promise = std::move(promise), username, email]() mutable {
            const bool ok = watcher->future().result();
            watcher->deleteLater();
            if (!ok) {
                promise.addResult(User());
                promise.finish();
                return;
            }
            promise.addResult(User(QStringLiteral("new"), username, email));
            promise.finish();
        });
        watcher->setFuture(createFuture);

        return future;
    }
    
    User currentUser() const { return m_currentUser; }
    bool isLoggedIn() const { return !m_currentUser.id().isEmpty(); }

    QString accessToken() const { return m_accessToken; }
    QString refreshToken() const { return m_refreshToken; }
    qint64 expiresIn() const { return m_expiresIn; }
    QString role() const { return m_role; }
    QStringList permissions() const { return m_permissions; }
    bool isAdmin() const { return m_role == QStringLiteral("admin"); }
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

    void createUserAsAdminImpl(const QString& fullName,
                              const QString& email,
                              const QString& password,
                              const QString& roleName,
                              bool hasRetried,
                              const std::shared_ptr<QPromise<bool>>& promise) {
        m_lastError.clear();

        if (m_accessToken.trimmed().isEmpty()) {
            m_lastError = QStringLiteral("Non authentifié");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString name = fullName.trimmed();
        const QString mail = email.trimmed();
        const QString role = roleName.trimmed().isEmpty() ? QStringLiteral("user") : roleName.trimmed();
        if (name.isEmpty() || mail.isEmpty() || password.isEmpty()) {
            m_lastError = QStringLiteral("Champs requis manquants");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QUrl url(m_gatewayBaseUrl + QStringLiteral("/register"));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(m_accessToken).toUtf8());

        const QJsonObject payload{{"fullName", name}, {"email", mail}, {"password", password}, {"roleName", role}};
        const QByteArray body = QJsonDocument(payload).toJson(QJsonDocument::Compact);

        QNetworkReply* reply = m_network.post(req, body);
        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, reply, name, mail, password, role, hasRetried, promise]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                promise->addResult(false);
                promise->finish();
            };

            // If access token expired, try refresh once and retry.
            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                auto* watcher = new QFutureWatcher<bool>(this);
                QObject::connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher, promise, name, mail, password, role]() {
                    const bool ok = watcher->future().result();
                    watcher->deleteLater();
                    if (!ok) {
                        m_lastError = m_lastError.isEmpty() ? QStringLiteral("Session expirée") : m_lastError;
                        promise->addResult(false);
                        promise->finish();
                        return;
                    }
                    createUserAsAdminImpl(name, mail, password, role, /*hasRetried*/ true, promise);
                });
                watcher->setFuture(refreshAccessToken());
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                const QString detail = QString::fromUtf8(raw).trimmed();
                const QString msg = detail.isEmpty() ? reply->errorString() : QStringLiteral("%1").arg(detail);
                reply->deleteLater();
                fail(QStringLiteral("Erreur création utilisateur (%1): %2").arg(httpStatus).arg(msg));
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
            const bool success = obj.value("success").toBool(false);
            const QString message = obj.value("message").toString();
            reply->deleteLater();

            if (!success) {
                fail(message.isEmpty() ? QStringLiteral("Création utilisateur échouée") : message);
                return;
            }

            promise->addResult(true);
            promise->finish();
        });
    }

    void deleteUserAsAdminImpl(const QString& userId,
                               bool hasRetried,
                               const std::shared_ptr<QPromise<bool>>& promise) {
        m_lastError.clear();

        if (!isAdmin()) {
            m_lastError = QStringLiteral("Accès refusé (admin requis)");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString token = m_accessToken.trimmed();
        if (token.isEmpty()) {
            m_lastError = QStringLiteral("Non authentifié");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QString uid = userId.trimmed();
        if (uid.isEmpty()) {
            m_lastError = QStringLiteral("user_id requis");
            promise->addResult(false);
            promise->finish();
            return;
        }

        const QUrl url(m_gatewayBaseUrl + QStringLiteral("/users/%1").arg(uid));
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
        req.setRawHeader("Authorization", QStringLiteral("Bearer %1").arg(token).toUtf8());

        QNetworkReply* reply = m_network.deleteResource(req);
        QObject::connect(reply, &QNetworkReply::finished, this,
                         [this, reply, uid, hasRetried, promise]() mutable {
            const int httpStatus = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray raw = reply->readAll();

            auto isHttpOk = [&](int status) {
                return status >= 200 && status < 300;
            };

            auto fail = [&](const QString& message) {
                m_lastError = message;
                promise->addResult(false);
                promise->finish();
            };

            if (httpStatus == 401 && !hasRetried) {
                reply->deleteLater();
                auto* watcher = new QFutureWatcher<bool>(this);
                QObject::connect(watcher, &QFutureWatcher<bool>::finished, this,
                                 [this, watcher, promise, uid]() {
                    const bool ok = watcher->future().result();
                    watcher->deleteLater();
                    if (!ok) {
                        m_lastError = m_lastError.isEmpty() ? QStringLiteral("Session expirée") : m_lastError;
                        promise->addResult(false);
                        promise->finish();
                        return;
                    }
                    deleteUserAsAdminImpl(uid, /*hasRetried*/ true, promise);
                });
                watcher->setFuture(refreshAccessToken());
                return;
            }

            if (reply->error() != QNetworkReply::NoError || !isHttpOk(httpStatus)) {
                const QString detail = QString::fromUtf8(raw).trimmed();
                const QString msg = detail.isEmpty() ? reply->errorString() : QStringLiteral("%1").arg(detail);
                reply->deleteLater();
                fail(QStringLiteral("Erreur suppression utilisateur (%1): %2").arg(httpStatus).arg(msg));
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
            const bool success = obj.value("success").toBool(false);
            const QString message = obj.value("message").toString();
            reply->deleteLater();

            if (!success) {
                fail(message.isEmpty() ? QStringLiteral("Suppression utilisateur échouée") : message);
                return;
            }

            promise->addResult(true);
            promise->finish();
        });
    }
    
    User m_currentUser;

    QNetworkAccessManager m_network;
    QString m_gatewayBaseUrl = QStringLiteral("http://localhost:8080");
    QString m_accessToken;
    QString m_refreshToken;
    qint64 m_expiresIn = 0;
    QString m_role;
    QStringList m_permissions;
    QString m_lastError;

    quint64 m_loginRequestSeq = 0;
};
