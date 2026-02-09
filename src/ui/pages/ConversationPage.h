#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include "../components/MessageBubble.h"
#include "../models/Contact.h"
#include "../models/Message.h"
#include "../utils/StyleHelper.h"

class ConversationPage : public QWidget {
    Q_OBJECT
    
public:
    explicit ConversationPage(QWidget* parent = nullptr) : QWidget(parent) {
        QVBoxLayout* root = new QVBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);
        
        // Header WhatsApp style
        QWidget* header = new QWidget(this);
        header->setFixedHeight(64);
        header->setStyleSheet("background:" + StyleHelper::white() + ";"
                             "border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 0, 24, 0);
        headerLayout->setSpacing(12);
        
        // Avatar circulaire du contact
        QLabel* avatar = new QLabel(this);
        avatar->setFixedSize(40, 40);
        avatar->setStyleSheet(
            "background:" + StyleHelper::primaryRed() + ";"
            "border-radius:20px;"
            "color:white;"
            "font-size:14px;"
            "font-weight:600;"
        );
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText("?");
        
        m_headerName = new QLabel("Conversation", this);
        m_headerName->setStyleSheet("font-size:16px;font-weight:600;color:" + StyleHelper::darkGray() + ";");
        
        headerLayout->addWidget(avatar);
        headerLayout->addWidget(m_headerName);
        headerLayout->addStretch();
        
        root->addWidget(header);
        
        // Messages area avec fond WhatsApp
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:" + StyleHelper::lightGray() + ";");
        
        QWidget* scrollContent = new QWidget;
        m_messagesLayout = new QVBoxLayout(scrollContent);
        m_messagesLayout->setSpacing(8);
        m_messagesLayout->setContentsMargins(24, 16, 24, 16);
        
        // Messages démo
        addMessage(Message("Bonjour, comment puis-je vous aider ?", Message::Type::Received));
        addMessage(Message("J'ai besoin d'informations sur la mission", Message::Type::Sent));
        addMessage(Message("Bien sûr, je vous envoie les documents", Message::Type::Received));
        
        m_messagesLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        
        root->addWidget(scrollArea, 1);
        
        // Input bar
        QWidget* inputBar = new QWidget(this);
        inputBar->setFixedHeight(64);
        inputBar->setStyleSheet("background:" + StyleHelper::white() + ";"
                                "border-top:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* inputLayout = new QHBoxLayout(inputBar);
        inputLayout->setContentsMargins(16, 8, 16, 8);
        inputLayout->setSpacing(8);
        
        QLineEdit* messageInput = new QLineEdit(inputBar);
        messageInput->setPlaceholderText("Écrire un message...");
        messageInput->setStyleSheet(
            "QLineEdit {"
            "  background:" + StyleHelper::lightGray() + ";"
            "  border:none;"
            "  border-radius:20px;"
            "  padding:10px 16px;"
            "  font-size:14px;"
            "}"
        );
        
        QPushButton* sendBtn = new QPushButton("➤", inputBar);
        sendBtn->setFixedSize(44, 44);
        sendBtn->setCursor(Qt::PointingHandCursor);
        sendBtn->setStyleSheet(
            "QPushButton {"
            "  background:" + StyleHelper::primaryRed() + ";"
            "  color:white;"
            "  border:none;"
            "  border-radius:22px;"
            "  font-size:18px;"
            "  font-weight:bold;"
            "}"
            "QPushButton:hover {"
            "  background:" + StyleHelper::primaryRed() + ";"
            "  opacity:0.9;"
            "}"
        );
        
        inputLayout->addWidget(messageInput, 1);
        inputLayout->addWidget(sendBtn);
        
        root->addWidget(inputBar);
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
