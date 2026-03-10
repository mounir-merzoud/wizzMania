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
    Message(const QString& content,
            Type type,
            const QDateTime& timestamp = QDateTime::currentDateTime(),
            const QString& senderId = QString(),
            const QString& messageId = QString())
        : m_content(content)
        , m_type(type)
        , m_timestamp(timestamp)
        , m_senderId(senderId)
        , m_messageId(messageId) {}
    
    QString content() const { return m_content; }
    Type type() const { return m_type; }
    QDateTime timestamp() const { return m_timestamp; }
    QString senderId() const { return m_senderId; }
    QString messageId() const { return m_messageId; }
    
    void setContent(const QString& content) { m_content = content; }
    void setType(Type type) { m_type = type; }
    void setTimestamp(const QDateTime& timestamp) { m_timestamp = timestamp; }
    void setSenderId(const QString& senderId) { m_senderId = senderId; }
    void setMessageId(const QString& messageId) { m_messageId = messageId; }
    
private:
    QString m_content;
    Type m_type;
    QDateTime m_timestamp;
    QString m_senderId;
    QString m_messageId;
};
