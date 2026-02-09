#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QList>
#include "../components/ContactRow.h"
#include "../models/Contact.h"
#include "../utils/StyleHelper.h"

class ContactsPage : public QWidget {
    Q_OBJECT
    
public:
    explicit ContactsPage(QWidget* parent = nullptr) : QWidget(parent) {
        QVBoxLayout* layout = new QVBoxLayout(this);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Header MSF style
        QWidget* header = new QWidget(this);
        header->setFixedHeight(64);
        header->setStyleSheet("background:" + StyleHelper::white() + ";"
                             "border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 0, 24, 0);
        
        QLabel* title = new QLabel("Conversations", header);
        title->setStyleSheet("font-size:20px;font-weight:600;color:" + StyleHelper::darkGray() + ";");
        
        headerLayout->addWidget(title);
        headerLayout->addStretch();
        
        layout->addWidget(header);
        
        // Search bar WhatsApp style
        QWidget* searchContainer = new QWidget(this);
        searchContainer->setStyleSheet("background:" + StyleHelper::white() + ";"
                                      "border-bottom:1px solid " + StyleHelper::borderGray() + ";"
                                      "padding:12px 16px;");
        
        QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
        searchLayout->setContentsMargins(8, 8, 8, 8);
        
        QLineEdit* searchBox = new QLineEdit(searchContainer);
        searchBox->setPlaceholderText("Rechercher...");
        searchBox->setStyleSheet(
            "QLineEdit {"
            "  background:" + StyleHelper::lightGray() + ";"
            "  border:none;"
            "  border-radius:18px;"
            "  padding:8px 16px;"
            "  font-size:14px;"
            "  color:" + StyleHelper::darkGray() + ";"
            "}"
            "QLineEdit:focus {"
            "  background:#E8E8E8;"
            "}"
        );
        
        searchLayout->addWidget(searchBox);
        layout->addWidget(searchContainer);
        
        // Scroll area for contacts
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:" + StyleHelper::white() + ";");
        
        QWidget* scrollContent = new QWidget;
        m_contactsLayout = new QVBoxLayout(scrollContent);
        m_contactsLayout->setSpacing(0);
        m_contactsLayout->setContentsMargins(0, 0, 0, 0);
        
        // Add contacts MSF style
        addContact(Contact("1", "Dr. Sarah Chen", "En ligne"));
        addContact(Contact("2", "Dr. Ahmed Hassan", "Hors ligne"));
        addContact(Contact("3", "Dr. Maria Rodriguez", "En ligne"));
        addContact(Contact("4", "Infirmière Sophie Dubois", "Occupé"));
        addContact(Contact("5", "Dr. John Smith", "Hors ligne"));
        addContact(Contact("6", "Coordinateur Pierre Martin", "En ligne"));
        
        m_contactsLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        
        layout->addWidget(scrollArea, 1);
    }
    
signals:
    void contactClicked(const Contact& contact);
    
private:
    void addContact(const Contact& contact) {
        ContactRow* row = new ContactRow(contact, this);
        connect(row, &ContactRow::clicked, this, &ContactsPage::contactClicked);
        m_contactsLayout->insertWidget(m_contactsLayout->count() - 1, row);
    }
    
    QVBoxLayout* m_contactsLayout;
};
