#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include "../models/Contact.h"
#include "../models/Message.h"

/**
 * @brief Service de messagerie
 * 
 * Ce service communiquera avec le microservice messaging_service via gRPC.
 * Pour l'instant, c'est un stub avec des données mockées.
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
    QList<Contact> getContacts() {
        // TODO: Implémenter l'appel gRPC vers messaging_service
        return {
            Contact("1", "Alice", "Online"),
            Contact("2", "Bob", "Away"),
            Contact("3", "Charlie", "Offline")
        };
    }
    
    /**
     * @brief Récupère les messages d'une conversation
     */
    QList<Message> getMessages(const QString& contactId) {
        // TODO: Implémenter l'appel gRPC
        Q_UNUSED(contactId);
        
        return {
            Message("Hi, how can I help you?", Message::Type::Received),
            Message("Can you share with me that file.", Message::Type::Sent)
        };
    }
    
    /**
     * @brief Envoie un message
     */
    void sendMessage(const QString& contactId, const QString& content) {
        // TODO: Implémenter l'appel gRPC
        Q_UNUSED(contactId);
        Q_UNUSED(content);
        
        emit messageSent(contactId, content);
    }
    
signals:
    void messageSent(const QString& contactId, const QString& content);
    void messageReceived(const QString& contactId, const Message& message);
    void contactStatusChanged(const QString& contactId, const QString& status);
    
private:
    MessagingService() = default;
    ~MessagingService() = default;
    MessagingService(const MessagingService&) = delete;
    MessagingService& operator=(const MessagingService&) = delete;
};
