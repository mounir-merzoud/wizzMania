#pragma once

#include <QWidget>
#include <QLabel>
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
        layout->setContentsMargins(0, 0, 0, 0);
        
        QLabel* bubble = new QLabel(message.content(), this);
        bubble->setWordWrap(true);
        bubble->setMaximumWidth(260);
        
        if (message.type() == Message::Type::Sent) {
            bubble->setStyleSheet(
                "background:#2563eb;"
                "color:white;"
                "border-radius:14px;"
                "padding:10px 14px;"
            );
            layout->addStretch();
            layout->addWidget(bubble, 0, Qt::AlignRight);
        } else {
            bubble->setStyleSheet(
                "background:white;"
                "color:#0b1120;"
                "border-radius:14px;"
                "padding:10px 14px;"
            );
            layout->addWidget(bubble, 0, Qt::AlignLeft);
            layout->addStretch();
        }
    }
    
    Message message() const { return m_message; }
    
private:
    Message m_message;
};
