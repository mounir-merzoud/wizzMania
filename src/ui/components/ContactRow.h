#pragma once

#include <QWidget>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include "../models/Contact.h"
#include "../utils/StyleHelper.h"

class ContactRow : public QWidget {
    Q_OBJECT
    
public:
    explicit ContactRow(const Contact& contact, QWidget* parent = nullptr)
        : QWidget(parent), m_contact(contact)
    {
        setFixedHeight(72);
        setCursor(Qt::PointingHandCursor);
        
        QHBoxLayout* layout = new QHBoxLayout(this);
        layout->setContentsMargins(16, 12, 16, 12);
        layout->setSpacing(12);
        
        // Avatar circulaire avec initiales
        QLabel* avatar = new QLabel(this);
        avatar->setFixedSize(48, 48);
        
        // Couleur avatar basée sur le nom
        QString avatarColor = getAvatarColor(contact.name());
        QString initials = getInitials(contact.name());
        
        avatar->setStyleSheet(
            "QLabel {"
            "  background:" + avatarColor + ";"
            "  border-radius:24px;"
            "  color:white;"
            "  font-size:16px;"
            "  font-weight:600;"
            "}"
        );
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setText(initials);
        
        // Name and status
        QWidget* textContainer = new QWidget(this);
        QVBoxLayout* textLayout = new QVBoxLayout(textContainer);
        textLayout->setContentsMargins(0, 0, 0, 0);
        textLayout->setSpacing(4);
        
        m_nameLabel = new QLabel(contact.name(), textContainer);
        m_nameLabel->setStyleSheet("font-size:15px;font-weight:500;color:" + StyleHelper::darkGray() + ";");
        
        m_statusLabel = new QLabel(contact.status(), textContainer);
        m_statusLabel->setStyleSheet("font-size:13px;color:" + StyleHelper::textLight() + ";");
        
        textLayout->addWidget(m_nameLabel);
        textLayout->addWidget(m_statusLabel);
        
        layout->addWidget(avatar);
        layout->addWidget(textContainer, 1);
        
        setStyleSheet(
            "ContactRow {"
            "  background:" + StyleHelper::white() + ";"
            "  border-bottom:1px solid " + StyleHelper::borderGray() + ";"
            "}"
            "ContactRow:hover {"
            "  background:" + StyleHelper::lightGray() + ";"
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
    QString getInitials(const QString& name) {
        QStringList parts = name.split(' ');
        if (parts.isEmpty()) return "?";
        
        QString initials;
        if (parts.size() == 1) {
            initials = parts[0].left(2).toUpper();
        } else {
            initials = parts[0].left(1).toUpper() + parts[1].left(1).toUpper();
        }
        return initials;
    }
    
    QString getAvatarColor(const QString& name) {
        // Palette de couleurs MSF-friendly
        QStringList colors = {
            "#0066CC", // Blue primary
            "#0052A3", // Darker blue
            "#2E86DE", // Sky blue
            "#00A8CC", // Cyan
            "#16A085", // Teal
            "#27AE60"  // Green
        };
        
        int hash = 0;
        for (QChar c : name) {
            hash += c.unicode();
        }
        
        return colors[hash % colors.size()];
    }
    
    Contact m_contact;
    QLabel* m_nameLabel;
    QLabel* m_statusLabel;
};
