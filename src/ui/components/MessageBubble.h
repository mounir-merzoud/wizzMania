#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "../models/Message.h"
#include "../utils/StyleHelper.h"

class MessageBubble : public QWidget {
    Q_OBJECT
    
public:
    explicit MessageBubble(const Message& message, QWidget* parent = nullptr)
        : QWidget(parent), m_message(message)
    {
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(0, 4, 0, 4);
        
        QWidget* bubbleContainer = new QWidget(this);
        QVBoxLayout* bubbleLayout = new QVBoxLayout(bubbleContainer);
        bubbleLayout->setContentsMargins(0, 0, 0, 0);
        bubbleLayout->setSpacing(4);
        
        QLabel* bubble = new QLabel(message.content(), this);
        bubble->setWordWrap(true);
        bubble->setMaximumWidth(400);
        bubble->setTextInteractionFlags(Qt::TextSelectableByMouse);
        
        QLabel* timestamp = new QLabel("12:34", this);
        timestamp->setStyleSheet("font-size:11px;color:" + StyleHelper::textGray() + ";");
        
        if (message.type() == Message::Type::Sent) {
            // Message envoyé - bulle bleue à droite
            bubble->setStyleSheet(
                "QLabel {"
                "  background:" + StyleHelper::primaryBlue() + ";"
                "  color:white;"
                "  border-radius:12px;"
                "  border-top-right-radius:2px;"
                "  padding:10px 14px;"
                "  font-size:14px;"
                "}"
            );
            bubbleLayout->addWidget(bubble, 0, Qt::AlignRight);
            bubbleLayout->addWidget(timestamp, 0, Qt::AlignRight);
            
            layout->addStretch();
            layout->addWidget(bubbleContainer, 0, Qt::AlignRight);
        } else {
            // Message reçu - bulle blanche à gauche
            bubble->setStyleSheet(
                "QLabel {"
                "  background:" + StyleHelper::white() + ";"
                "  color:" + StyleHelper::textDark() + ";"
                "  border-radius:12px;"
                "  border-top-left-radius:2px;"
                "  padding:10px 14px;"
                "  font-size:14px;"
                "  border:1px solid " + StyleHelper::borderGray() + ";"
                "}"
            );
            bubbleLayout->addWidget(bubble, 0, Qt::AlignLeft);
            bubbleLayout->addWidget(timestamp, 0, Qt::AlignLeft);
            
            layout->addWidget(bubbleContainer, 0, Qt::AlignLeft);
            layout->addStretch();
        }
    }
    
    Message message() const { return m_message; }
    
private:
    Message m_message;
};
