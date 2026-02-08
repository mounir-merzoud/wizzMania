#pragma once

#include <QObject>
#include <QString>
#include <QFuture>
#include "../models/User.h"

/**
 * @brief Service d'authentification
 * 
 * Ce service communiquera avec le microservice auth_service via gRPC.
 * Pour l'instant, c'est un stub avec des données mockées.
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
        // TODO: Implémenter l'appel gRPC vers auth_service
        // Pour l'instant, retourne un utilisateur mocké
        Q_UNUSED(password);
        
        User user("1", username, username + "@wizzmania.com");
        return QtFuture::makeReadyFuture(user);
    }
    
    /**
     * @brief Déconnecte l'utilisateur actuel
     */
    void logout() {
        // TODO: Implémenter l'appel gRPC
        m_currentUser = User();
        emit userLoggedOut();
    }
    
    /**
     * @brief Inscrit un nouvel utilisateur
     */
    QFuture<User> registerUser(const QString& username, const QString& email, const QString& password) {
        // TODO: Implémenter l'appel gRPC vers auth_service
        Q_UNUSED(password);
        
        User user("new", username, email);
        return QtFuture::makeReadyFuture(user);
    }
    
    User currentUser() const { return m_currentUser; }
    bool isLoggedIn() const { return !m_currentUser.id().isEmpty(); }
    
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
};
