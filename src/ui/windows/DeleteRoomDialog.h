#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QMessageBox>
#include <QFutureWatcher>

#include "../../services/MessagingService.h"
#include "../../services/AuthService.h"
#include "../../utils/StyleHelper.h"

class DeleteRoomDialog : public QDialog {
    Q_OBJECT

public:
    explicit DeleteRoomDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Supprimer un salon");
        setModal(true);
        setFixedSize(520, 420);

        setStyleSheet("QDialog { background:" + StyleHelper::white() + "; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(14);

        auto* title = new QLabel("Supprimer un salon", this);
        title->setStyleSheet(StyleHelper::titleStyle());
        root->addWidget(title);

        auto* subtitle = new QLabel("Action réservée aux administrateurs", this);
        subtitle->setStyleSheet(StyleHelper::secondaryTextStyle());
        root->addWidget(subtitle);

        m_list = new QListWidget(this);
        m_list->setStyleSheet(
            "QListWidget {"
            "  background:" + StyleHelper::white() + ";"
            "  border:1px solid " + StyleHelper::borderGray() + ";"
            "  border-radius:10px;"
            "  padding:6px;"
            "}"
            "QListWidget::item { padding:10px; border-radius:8px; }"
            "QListWidget::item:selected { background:" + StyleHelper::lightGray() + "; }"
        );
        root->addWidget(m_list, 1);

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

        m_delete = new QPushButton("Supprimer", this);
        m_delete->setStyleSheet(StyleHelper::primaryButton());
        m_delete->setCursor(Qt::PointingHandCursor);
        connect(m_delete, &QPushButton::clicked, this, &DeleteRoomDialog::onDeleteClicked);

        buttons->addStretch();
        buttons->addWidget(m_cancel);
        buttons->addWidget(m_delete);
        root->addLayout(buttons);

        connect(&MessagingService::instance(), &MessagingService::contactsUpdated, this, [this]() {
            populateFromContacts();
        });

        MessagingService::instance().refreshContacts();
        populateFromContacts();
    }

signals:
    void roomDeleted();

private slots:
    void onDeleteClicked() {
        m_error->hide();

        if (!AuthService::instance().isAdmin()) {
            showError("Accès refusé (admin requis). ");
            return;
        }

        auto* item = m_list->currentItem();
        if (!item) {
            showError("Sélectionne un salon.");
            return;
        }

        const QString roomId = item->data(Qt::UserRole).toString();
        if (roomId.isEmpty() || !roomId.startsWith(QStringLiteral("room:"))) {
            showError("room_id invalide");
            return;
        }

        const auto answer = QMessageBox::warning(
            this,
            QStringLiteral("Confirmation"),
            QStringLiteral("Supprimer ce salon ? Tous les messages seront supprimés."),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No
        );
        if (answer != QMessageBox::Yes) {
            return;
        }

        setUiBusy(true);

        auto future = MessagingService::instance().deleteRoomAsAdmin(roomId);
        auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            const bool ok = watcher->future().result();
            watcher->deleteLater();
            setUiBusy(false);

            if (!ok) {
                const QString msg = MessagingService::instance().lastError().isEmpty()
                                        ? QStringLiteral("Suppression salon échouée")
                                        : MessagingService::instance().lastError();
                showError(msg);
                return;
            }

            MessagingService::instance().refreshContacts();
            emit roomDeleted();
            accept();
        });
        watcher->setFuture(future);
    }

private:
    void populateFromContacts() {
        m_list->clear();
        const auto contacts = MessagingService::instance().getContacts();

        int count = 0;
        for (const auto& c : contacts) {
            const QString id = c.id();
            if (!id.startsWith(QStringLiteral("room:"))) {
                continue;
            }

            const QString label = c.name().isEmpty() ? id : c.name();
            auto* item = new QListWidgetItem(label, m_list);
            item->setData(Qt::UserRole, id);
            ++count;
        }

        if (count == 0) {
            auto* item = new QListWidgetItem("Aucun salon disponible (charge /rooms)", m_list);
            item->setFlags(Qt::NoItemFlags);
        }
    }

    void showError(const QString& msg) {
        m_error->setText(msg);
        m_error->show();
    }

    void setUiBusy(bool busy) {
        m_list->setEnabled(!busy);
        m_cancel->setEnabled(!busy);
        m_delete->setEnabled(!busy);
    }

    QListWidget* m_list = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_delete = nullptr;
};
