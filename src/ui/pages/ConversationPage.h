#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include "../components/MessageBubble.h"
#include "../models/Contact.h"
#include "../models/Message.h"
#include "../utils/StyleHelper.h"

class ConversationPage : public QWidget {
    Q_OBJECT
    
public:
    explicit ConversationPage(QWidget* parent = nullptr) : QWidget(parent) {
        QVBoxLayout* root = new QVBoxLayout(this);
        root->setContentsMargins(40, 32, 40, 32);
        root->setSpacing(16);
        
        m_headerName = new QLabel("Conversation", this);
        m_headerName->setStyleSheet(StyleHelper::subtitleStyle());
        
        // Scroll area for messages
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:transparent;");
        
        QWidget* scrollContent = new QWidget;
        m_messagesLayout = new QVBoxLayout(scrollContent);
        m_messagesLayout->setSpacing(8);
        m_messagesLayout->setContentsMargins(0, 0, 0, 0);
        
        // Add fake messages
        addMessage(Message("Hi, how can I help you?", Message::Type::Received));
        addMessage(Message("Can you share with me that file.", Message::Type::Sent));
        
        m_messagesLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        
        root->addWidget(m_headerName);
        root->addWidget(scrollArea, 1);
    }
    
    void setContact(const Contact& contact) {
        m_headerName->setText(contact.name());
    }
    
private:
    void addMessage(const Message& message) {
        MessageBubble* bubble = new MessageBubble(message, this);
        m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    }
    
    QLabel* m_headerName;
    QVBoxLayout* m_messagesLayout;
};
