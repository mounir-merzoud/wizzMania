#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QFutureWatcher>
#include <QRegularExpression>
#include <QSet>

#include "../../services/AuthService.h"
#include "../../services/MessagingService.h"
#include "../../utils/StyleHelper.h"

class CreateRoomDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateRoomDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Créer un salon");
        setModal(true);
        setFixedSize(520, 360);

        setStyleSheet("QDialog { background:" + StyleHelper::white() + "; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(14);

        auto* title = new QLabel("Créer un salon", this);
        title->setStyleSheet(StyleHelper::titleStyle());
        root->addWidget(title);

        auto* subtitle = new QLabel("Action réservée aux administrateurs", this);
        subtitle->setStyleSheet(StyleHelper::secondaryTextStyle());
        root->addWidget(subtitle);

        m_title = new QLineEdit(this);
        m_title->setPlaceholderText("Titre du salon");
        m_title->setStyleSheet(StyleHelper::inputStyle());
        root->addWidget(m_title);

        auto* participantsLabel = new QLabel("Participants", this);
        participantsLabel->setStyleSheet(StyleHelper::secondaryTextStyle());
        root->addWidget(participantsLabel);

        m_userList = new QListWidget(this);
        m_userList->setSelectionMode(QAbstractItemView::NoSelection);
        m_userList->setStyleSheet(
            "QListWidget {"
            "  background:" + StyleHelper::white() + ";"
            "  border:1px solid " + StyleHelper::borderGray() + ";"
            "  border-radius:10px;"
            "  padding:6px;"
            "  font-size:14px;"
            "  color:" + StyleHelper::darkGray() + ";"
            "}"
            "QListWidget::item {"
            "  padding:8px 10px;"
            "}"
        );
        m_userList->setMinimumHeight(170);
        root->addWidget(m_userList);

        // Populate list from cached contacts; also refresh asynchronously.
        populateUsers();
        connect(&MessagingService::instance(), &MessagingService::contactsUpdated, this, [this]() {
            populateUsers();
        });
        MessagingService::instance().refreshContacts();

        m_error = new QLabel(this);
        m_error->setStyleSheet("color:#DC3545;font-size:13px;");
        m_error->setWordWrap(true);
        m_error->hide();
        root->addWidget(m_error);

        auto* buttons = new QHBoxLayout();
        buttons->setSpacing(10);

        m_cancel = new QPushButton("Annuler", this);
        m_cancel->setStyleSheet(StyleHelper::secondaryButton());
        m_cancel->setCursor(Qt::PointingHandCursor);
        connect(m_cancel, &QPushButton::clicked, this, &QDialog::reject);

        m_create = new QPushButton("Créer", this);
        m_create->setStyleSheet(StyleHelper::primaryButton());
        m_create->setCursor(Qt::PointingHandCursor);
        connect(m_create, &QPushButton::clicked, this, &CreateRoomDialog::onCreateClicked);

        buttons->addStretch();
        buttons->addWidget(m_cancel);
        buttons->addWidget(m_create);
        root->addLayout(buttons);
    }

signals:
    void roomCreated();

private slots:
    void onCreateClicked() {
        m_error->hide();

        if (!AuthService::instance().isAdmin()) {
            showError("Accès refusé (admin requis). ");
            return;
        }

        const QString title = m_title->text().trimmed();

        QStringList emails;
        for (int i = 0; i < m_userList->count(); ++i) {
            auto* item = m_userList->item(i);
            if (!item) continue;
            if (item->checkState() != Qt::Checked) continue;
            const QString email = item->data(Qt::UserRole).toString().trimmed();
            if (!email.isEmpty()) {
                emails.push_back(email);
            }
        }

        if (emails.isEmpty()) {
            showError("Sélectionne au moins un participant.");
            return;
        }

        setUiBusy(true);

        auto future = MessagingService::instance().createRoomAsAdmin(title, emails);
        auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            const bool ok = watcher->future().result();
            watcher->deleteLater();
            setUiBusy(false);

            if (!ok) {
                const QString msg = MessagingService::instance().lastError().isEmpty()
                                        ? QStringLiteral("Création salon échouée")
                                        : MessagingService::instance().lastError();
                showError(msg);
                return;
            }

            emit roomCreated();
            accept();
        });
        watcher->setFuture(future);
    }

private:
    void showError(const QString& msg) {
        m_error->setText(msg);
        m_error->show();
    }

    void setUiBusy(bool busy) {
        m_title->setEnabled(!busy);
        m_userList->setEnabled(!busy);
        m_cancel->setEnabled(!busy);
        m_create->setEnabled(!busy);
    }

    void populateUsers() {
        // Preserve existing checked emails.
        QSet<QString> checked;
        for (int i = 0; i < m_userList->count(); ++i) {
            auto* it = m_userList->item(i);
            if (!it) continue;
            if (it->checkState() == Qt::Checked) {
                checked.insert(it->data(Qt::UserRole).toString());
            }
        }

        m_userList->clear();

        const auto contacts = MessagingService::instance().getContacts();
        for (const auto& c : contacts) {
            // We only list actual users (rooms have status == "Room" and no email)
            if (c.status() == QStringLiteral("Room")) continue;

            const QString email = c.email().trimmed();
            if (email.isEmpty()) continue;

            const QString label = c.name().trimmed().isEmpty()
                                      ? email
                                      : QStringLiteral("%1 — %2").arg(c.name().trimmed(), email);

            auto* item = new QListWidgetItem(label, m_userList);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(checked.contains(email) ? Qt::Checked : Qt::Unchecked);
            item->setData(Qt::UserRole, email);
        }

        if (m_userList->count() == 0) {
            auto* item = new QListWidgetItem("Aucun utilisateur disponible (charge /users)", m_userList);
            item->setFlags(item->flags() & ~Qt::ItemIsEnabled);
        }
    }

    QLineEdit* m_title = nullptr;
    QListWidget* m_userList = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_create = nullptr;
};
