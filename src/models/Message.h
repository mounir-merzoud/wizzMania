#pragma once

#include <QString>
#include <QDateTime>

class Message {
public:
    enum class Type {
        Sent,
        Received
    };
    
    Message() = default;
    Message(const QString& content, Type type, const QDateTime& timestamp = QDateTime::currentDateTime())
        : m_content(content), m_type(type), m_timestamp(timestamp) {}
    
    QString content() const { return m_content; }
    Type type() const { return m_type; }
    QDateTime timestamp() const { return m_timestamp; }
    
    void setContent(const QString& content) { m_content = content; }
    void setType(Type type) { m_type = type; }
    void setTimestamp(const QDateTime& timestamp) { m_timestamp = timestamp; }
    
private:
    QString m_content;
    Type m_type;
    QDateTime m_timestamp;
};
