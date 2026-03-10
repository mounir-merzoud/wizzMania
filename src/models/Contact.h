#pragma once

#include <QString>

class Contact {
public:
    Contact() = default;
    Contact(const QString& id, const QString& name, const QString& status = "Offline", const QString& email = QString())
        : m_id(id), m_name(name), m_status(status), m_email(email) {}
    
    QString id() const { return m_id; }
    QString name() const { return m_name; }
    QString status() const { return m_status; }
    QString email() const { return m_email; }
    
    void setId(const QString& id) { m_id = id; }
    void setName(const QString& name) { m_name = name; }
    void setStatus(const QString& status) { m_status = status; }
    void setEmail(const QString& email) { m_email = email; }
    
private:
    QString m_id;
    QString m_name;
    QString m_status;
    QString m_email;
};
