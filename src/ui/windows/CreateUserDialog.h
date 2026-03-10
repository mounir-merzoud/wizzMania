#pragma once

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QFutureWatcher>

#include "../../services/AuthService.h"
#include "../../utils/StyleHelper.h"

class CreateUserDialog : public QDialog {
    Q_OBJECT

public:
    explicit CreateUserDialog(QWidget* parent = nullptr) : QDialog(parent) {
        setWindowTitle("Créer un utilisateur");
        setModal(true);
        setFixedSize(520, 360);

        setStyleSheet("QDialog { background:" + StyleHelper::white() + "; }");

        auto* root = new QVBoxLayout(this);
        root->setContentsMargins(24, 24, 24, 24);
        root->setSpacing(14);

        auto* title = new QLabel("Créer un utilisateur", this);
        title->setStyleSheet(StyleHelper::titleStyle());
        root->addWidget(title);

        auto* subtitle = new QLabel("Action réservée aux administrateurs", this);
        subtitle->setStyleSheet(StyleHelper::secondaryTextStyle());
        root->addWidget(subtitle);

        m_fullName = new QLineEdit(this);
        m_fullName->setPlaceholderText("Nom complet");
        m_fullName->setStyleSheet(StyleHelper::inputStyle());

        m_email = new QLineEdit(this);
        m_email->setPlaceholderText("Email");
        m_email->setStyleSheet(StyleHelper::inputStyle());

        m_password = new QLineEdit(this);
        m_password->setPlaceholderText("Mot de passe");
        m_password->setEchoMode(QLineEdit::Password);
        m_password->setStyleSheet(StyleHelper::inputStyle());

        m_role = new QComboBox(this);
        m_role->addItem("user");
        m_role->addItem("admin");
        m_role->setStyleSheet(
            "QComboBox {"
            "  background:" + StyleHelper::white() + ";"
            "  border:1px solid " + StyleHelper::borderGray() + ";"
            "  border-radius:10px;"
            "  padding:8px 12px;"
            "  font-size:14px;"
            "  color:" + StyleHelper::darkGray() + ";"
            "}"
            "QComboBox::drop-down { border:none; width:26px; }"
        );

        root->addWidget(m_fullName);
        root->addWidget(m_email);
        root->addWidget(m_password);
        root->addWidget(m_role);

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
        connect(m_create, &QPushButton::clicked, this, &CreateUserDialog::onCreateClicked);

        buttons->addStretch();
        buttons->addWidget(m_cancel);
        buttons->addWidget(m_create);

        root->addLayout(buttons);
    }

signals:
    void userCreated();

private slots:
    void onCreateClicked() {
        m_error->hide();

        if (!AuthService::instance().isAdmin()) {
            showError("Accès refusé (admin requis). ");
            return;
        }

        const QString fullName = m_fullName->text().trimmed();
        const QString email = m_email->text().trimmed();
        const QString password = m_password->text();
        const QString role = m_role->currentText().trimmed();

        setUiBusy(true);

        auto future = AuthService::instance().createUserAsAdmin(fullName, email, password, role);
        auto* watcher = new QFutureWatcher<bool>(this);
        connect(watcher, &QFutureWatcher<bool>::finished, this, [this, watcher]() {
            const bool ok = watcher->future().result();
            watcher->deleteLater();
            setUiBusy(false);

            if (!ok) {
                const QString msg = AuthService::instance().lastError().isEmpty()
                                        ? QStringLiteral("Création utilisateur échouée")
                                        : AuthService::instance().lastError();
                showError(msg);
                return;
            }

            emit userCreated();
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
        m_fullName->setEnabled(!busy);
        m_email->setEnabled(!busy);
        m_password->setEnabled(!busy);
        m_role->setEnabled(!busy);
        m_cancel->setEnabled(!busy);
        m_create->setEnabled(!busy);
    }

    QLineEdit* m_fullName = nullptr;
    QLineEdit* m_email = nullptr;
    QLineEdit* m_password = nullptr;
    QComboBox* m_role = nullptr;
    QLabel* m_error = nullptr;
    QPushButton* m_cancel = nullptr;
    QPushButton* m_create = nullptr;
};
