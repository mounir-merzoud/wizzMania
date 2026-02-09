#pragma once

#include <QString>

class User {
public:
    User() = default;
    User(const QString& id, const QString& username, const QString& email)
        : m_id(id), m_username(username), m_email(email) {}
    
    QString id() const { return m_id; }
    QString username() const { return m_username; }
    QString email() const { return m_email; }
    
    void setId(const QString& id) { m_id = id; }
    void setUsername(const QString& username) { m_username = username; }
    void setEmail(const QString& email) { m_email = email; }
    
private:
    QString m_id;
    QString m_username;
    QString m_email;
};
