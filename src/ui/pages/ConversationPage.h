#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QPushButton>
#include "../../services/MessagingService.h"
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

        m_errorLabel = new QLabel(this);
        m_errorLabel->setStyleSheet("color:#DC3545;font-size:13px;padding:8px 16px;background:" + StyleHelper::white() + ";border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        m_errorLabel->setWordWrap(true);
        m_errorLabel->hide();
        root->addWidget(m_errorLabel);
        
        // Messages area avec fond WhatsApp
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:" + StyleHelper::lightGray() + ";");
        
        QWidget* scrollContent = new QWidget;
        m_messagesLayout = new QVBoxLayout(scrollContent);
        m_messagesLayout->setSpacing(8);
        m_messagesLayout->setContentsMargins(24, 16, 24, 16);
        
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
        
        m_messageInput = new QLineEdit(inputBar);
        m_messageInput->setPlaceholderText("Écrire un message...");
        m_messageInput->setStyleSheet(
            "QLineEdit {"
            "  background:" + StyleHelper::lightGray() + ";"
            "  border:none;"
            "  border-radius:20px;"
            "  padding:10px 16px;"
            "  font-size:14px;"
            "}"
        );
        
        m_sendBtn = new QPushButton("➤", inputBar);
        m_sendBtn->setFixedSize(44, 44);
        m_sendBtn->setCursor(Qt::PointingHandCursor);
        m_sendBtn->setStyleSheet(
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
        
        inputLayout->addWidget(m_messageInput, 1);
        inputLayout->addWidget(m_sendBtn);
        
        root->addWidget(inputBar);

        connect(m_sendBtn, &QPushButton::clicked, this, &ConversationPage::onSendClicked);
        connect(m_messageInput, &QLineEdit::returnPressed, this, &ConversationPage::onSendClicked);
        connect(&MessagingService::instance(), &MessagingService::messageReceived,
                this, &ConversationPage::onMessageReceived);
        connect(&MessagingService::instance(), &MessagingService::messagesUpdated,
                this, [this](const QString& conversationId) {
                    if (!m_currentContact.id().isEmpty() && m_currentContact.id() == conversationId) {
                        reloadMessages();
                    }
                });
        connect(&MessagingService::instance(), &MessagingService::errorOccurred,
                this, [this](const QString& err) {
                    if (!m_errorLabel) return;
                    m_errorLabel->setText(err);
                    m_errorLabel->show();
                });
    }
    
    void setContact(const Contact& contact) {
        m_currentContact = contact;
        m_headerName->setText(contact.name());
        if (m_errorLabel) m_errorLabel->hide();
        reloadMessages();
        MessagingService::instance().refreshMessages(contact.id());
    }
    
private:
    void reloadMessages() {
        clearMessages();

        if (m_currentContact.id().isEmpty()) {
            return;
        }

        const auto messages = MessagingService::instance().getMessages(m_currentContact.id());
        for (const auto& msg : messages) {
            addMessage(msg);
        }
    }

    void clearMessages() {
        // Remove old bubbles (keep the final stretch item)
        while (m_messagesLayout->count() > 1) {
            QLayoutItem* item = m_messagesLayout->takeAt(0);
            if (!item) {
                break;
            }
            if (auto* w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
    }

    void addMessage(const Message& message) {
        MessageBubble* bubble = new MessageBubble(message, this);
        m_messagesLayout->insertWidget(m_messagesLayout->count() - 1, bubble);
    }

    void onSendClicked() {
        if (m_currentContact.id().isEmpty()) {
            return;
        }

        const QString content = m_messageInput->text().trimmed();
        if (content.isEmpty()) {
            return;
        }

        MessagingService::instance().sendMessage(m_currentContact.id(), content);
        addMessage(Message(content, Message::Type::Sent));
        m_messageInput->clear();
    }

    void onMessageReceived(const QString& contactId, const Message& message) {
        if (m_currentContact.id() != contactId) {
            return;
        }
        addMessage(message);
    }
    
    QLabel* m_headerName;
    QVBoxLayout* m_messagesLayout;
    QLabel* m_errorLabel = nullptr;

    Contact m_currentContact;
    QLineEdit* m_messageInput = nullptr;
    QPushButton* m_sendBtn = nullptr;
};
