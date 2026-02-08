#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QMouseEvent>
#include "../models/Contact.h"
#include "../utils/StyleHelper.h"

class ContactRow : public QWidget {
    Q_OBJECT
    
public:
    explicit ContactRow(const Contact& contact, QWidget* parent = nullptr)
        : QWidget(parent), m_contact(contact)
    {
        setFixedHeight(64);
        setCursor(Qt::PointingHandCursor);
        
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 8, 16, 8);
        layout->setSpacing(12);
        
        // Avatar circle
        QLabel* avatar = new QLabel(this);
        avatar->setFixedSize(48, 48);
        avatar->setStyleSheet(
            "background:" + StyleHelper::primaryBlue() + ";"
            "border-radius:24px;"
            "color:white;"
            "font-size:18px;"
            "font-weight:600;"
        );
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(contact.name().left(1).toUpper());
        
        // Name and status
        QWidget* textContainer = new QWidget(this);
        QVBoxLayout* textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(2);
        
        m_nameLabel = new QLabel(contact.name(), textContainer);
        m_nameLabel->setStyleSheet("font-size:16px;font-weight:600;color:#0b1120;");
        
        m_statusLabel = new QLabel(contact.status(), textContainer);
        m_statusLabel->setStyleSheet("font-size:13px;color:#6b7280;");
        
        textLayout->addWidget(m_nameLabel);
        textLayout->addWidget(m_statusLabel);
        textLayout->addStretch();
        
        layout->addWidget(avatar);
        layout->addWidget(textContainer, 1);
        
        setStyleSheet(
            "ContactRow {"
            "  background:white;"
            "  border-radius:12px;"
            "}"
            "ContactRow:hover {"
            "  background:#f3f4f6;"
            "}"
        );
    }
    
    Contact contact() const { return m_contact; }
    
signals:
    void clicked(const Contact& contact);
    
protected:
    void mousePressEvent(QMouseEvent* event) override {
        if (event->button() == Qt::LeftButton) {
            emit clicked(m_contact);
        }
        QWidget::mousePressEvent(event);
    }
    
private:
    Contact m_contact;
    QLabel* m_nameLabel;
    QLabel* m_statusLabel;
};
