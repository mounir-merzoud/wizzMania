#pragma once

#include <QWidget>
#include <QVBoxLayout>
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
        layout->setContentsMargins(40, 32, 40, 32);
        layout->setSpacing(16);
        
        QLabel* title = new QLabel("Contacts", this);
        title->setStyleSheet(StyleHelper::titleStyle());
        
        QLineEdit* searchBox = new QLineEdit(this);
        searchBox->setPlaceholderText("Search contacts...");
        searchBox->setStyleSheet(StyleHelper::inputStyle());
        
        // Scroll area for contacts
        QScrollArea* scrollArea = new QScrollArea(this);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:transparent;");
        
        QWidget* scrollContent = new QWidget;
        m_contactsLayout = new QVBoxLayout(scrollContent);
        m_contactsLayout->setSpacing(8);
        m_contactsLayout->setContentsMargins(0, 0, 0, 0);
        
        // Add some fake contacts
        addContact(Contact("1", "Alice", "Online"));
        addContact(Contact("2", "Bob", "Away"));
        addContact(Contact("3", "Charlie", "Offline"));
        
        m_contactsLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        
        layout->addWidget(title);
        layout->addWidget(searchBox);
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
