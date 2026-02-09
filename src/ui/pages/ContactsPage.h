#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QList>
#include <QTimer>
#include <QHash>
#include "../components/ContactRow.h"
#include "../models/Contact.h"
#include "../../services/MessagingService.h"
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

        m_errorLabel = new QLabel(this);
        m_errorLabel->setStyleSheet("color:#DC3545;font-size:13px;padding:8px 16px;background:" + StyleHelper::white() + ";border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        m_errorLabel->setWordWrap(true);
        m_errorLabel->hide();
        layout->addWidget(m_errorLabel);
        
        // Search bar WhatsApp style
        QWidget* searchContainer = new QWidget(this);
        searchContainer->setStyleSheet("background:" + StyleHelper::white() + ";"
                                      "border-bottom:1px solid " + StyleHelper::borderGray() + ";"
                                      "padding:12px 16px;");
        
        QHBoxLayout* searchLayout = new QHBoxLayout(searchContainer);
        searchLayout->setContentsMargins(8, 8, 8, 8);
        
        m_searchBox = new QLineEdit(searchContainer);
        m_searchBox->setPlaceholderText("Rechercher...");
        m_searchBox->setStyleSheet(
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
        
        searchLayout->addWidget(m_searchBox);
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

        m_contactsLayout->addStretch();
        scrollArea->setWidget(scrollContent);
        
        layout->addWidget(scrollArea, 1);

        connect(m_searchBox, &QLineEdit::textChanged, this, &ContactsPage::applyFilter);
        connect(&MessagingService::instance(), &MessagingService::contactStatusChanged,
            this, &ContactsPage::onContactStatusChanged);
        connect(&MessagingService::instance(), &MessagingService::contactsUpdated,
            this, &ContactsPage::reloadContacts);
        connect(&MessagingService::instance(), &MessagingService::errorOccurred,
            this, [this](const QString& err) {
                if (!m_errorLabel) return;
                m_errorLabel->setText(err);
                m_errorLabel->show();
            });

        reloadContacts();
        MessagingService::instance().refreshContacts();

        auto* timer = new QTimer(this);
        timer->setInterval(5000);
        connect(timer, &QTimer::timeout, &MessagingService::instance(), &MessagingService::refreshContacts);
        timer->start();
    }
    
signals:
    void contactClicked(const Contact& contact);
    
private:
    void reloadContacts() {
        m_allContacts = MessagingService::instance().getContacts();
        if (m_errorLabel) m_errorLabel->hide();
        applyFilter(m_searchBox ? m_searchBox->text() : QString());
    }

    void applyFilter(const QString& text) {
        const QString needle = text.trimmed();
        QList<Contact> filtered;
        filtered.reserve(m_allContacts.size());
        for (const auto& contact : m_allContacts) {
            if (needle.isEmpty() || contact.name().contains(needle, Qt::CaseInsensitive)) {
                filtered.push_back(contact);
            }
        }
        rebuildList(filtered);
    }

    void rebuildList(const QList<Contact>& contacts) {
        // Remove old rows (keep the final stretch item)
        while (m_contactsLayout->count() > 1) {
            QLayoutItem* item = m_contactsLayout->takeAt(0);
            if (!item) {
                break;
            }
            if (auto* w = item->widget()) {
                w->deleteLater();
            }
            delete item;
        }
        m_rowsById.clear();

        for (const auto& contact : contacts) {
            auto* row = new ContactRow(contact, this);
            connect(row, &ContactRow::clicked, this, &ContactsPage::contactClicked);
            m_contactsLayout->insertWidget(m_contactsLayout->count() - 1, row);
            m_rowsById.insert(contact.id(), row);
        }
    }

    void onContactStatusChanged(const QString& contactId, const QString& status) {
        if (!m_rowsById.contains(contactId)) {
            return;
        }

        ContactRow* row = m_rowsById.value(contactId);
        Contact c = row->contact();
        c.setStatus(status);
        row->setContact(c);
    }

    QVBoxLayout* m_contactsLayout = nullptr;
    QLineEdit* m_searchBox = nullptr;
    QLabel* m_errorLabel = nullptr;
    QList<Contact> m_allContacts;
    QHash<QString, ContactRow*> m_rowsById;
};
