#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include <QFutureWatcher>
#include "../../services/AuthService.h"
#include "../utils/StyleHelper.h"

class LoginWindow : public QWidget {
    Q_OBJECT
    
public:
    explicit LoginWindow(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("MSF Messenger - Login");
        setFixedSize(450, 600);
        
        setStyleSheet("QWidget { background:" + StyleHelper::white() + "; }");
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(40, 60, 40, 60);
        mainLayout->setSpacing(32);
        
        // Logo MSF (texte simple et épuré)
        QLabel* logo = new QLabel("MSF", this);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet(
            "font-size:48px;"
            "font-weight:700;"
            "color:" + StyleHelper::primaryBlue() + ";"
            "letter-spacing:4px;"
        );
        
        QLabel* subtitle = new QLabel("Médecins Sans Frontières", this);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet(StyleHelper::secondaryTextStyle());
        
        QLabel* appName = new QLabel("Secure Messenger", this);
        appName->setAlignment(Qt::AlignCenter);
        appName->setStyleSheet(
            "font-size:20px;"
            "font-weight:600;"
            "color:" + StyleHelper::textDark() + ";"
            "margin-top:8px;"
        );
        
        mainLayout->addWidget(logo);
        mainLayout->addWidget(subtitle);
        mainLayout->addWidget(appName);
        mainLayout->addSpacing(32);
        
        // Login form - très épuré
        m_usernameInput = new QLineEdit(this);
        m_usernameInput->setPlaceholderText("Email or username");
        m_usernameInput->setStyleSheet(StyleHelper::inputStyle());
        
        m_passwordInput = new QLineEdit(this);
        m_passwordInput->setPlaceholderText("Password");
        m_passwordInput->setEchoMode(QLineEdit::Password);
        m_passwordInput->setStyleSheet(StyleHelper::inputStyle());
        
        m_loginBtn = new QPushButton("Sign In", this);
        m_loginBtn->setStyleSheet(StyleHelper::primaryButton());
        m_loginBtn->setMinimumHeight(48);
        m_loginBtn->setCursor(Qt::PointingHandCursor);
        
        connect(m_loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
        
        // Error label
        m_errorLabel = new QLabel(this);
        m_errorLabel->setStyleSheet("color:#DC3545;font-size:14px;");
        m_errorLabel->setAlignment(Qt::AlignCenter);
        m_errorLabel->setWordWrap(true);
        m_errorLabel->hide();
        
        mainLayout->addWidget(m_usernameInput);
        mainLayout->addWidget(m_passwordInput);
        mainLayout->addWidget(m_errorLabel);
        mainLayout->addWidget(m_loginBtn);
        mainLayout->addStretch();
        
        // Footer discret
        QLabel* footer = new QLabel("Secure communication for humanitarian field workers", this);
        footer->setAlignment(Qt::AlignCenter);
        footer->setStyleSheet(
            "font-size:12px;"
            "color:" + StyleHelper::textGray() + ";"
        );
        footer->setWordWrap(true);
        
        mainLayout->addWidget(footer);
    }
    
signals:
    void loginSuccessful(const QString& username);
    
private slots:
    void onLoginClicked() {
        QString username = m_usernameInput->text().trimmed();
        QString password = m_passwordInput->text();
        
        m_errorLabel->hide();
        
        if (username.isEmpty() || password.isEmpty()) {
            m_errorLabel->setText("Veuillez remplir tous les champs");
            m_errorLabel->show();
            return;
        }

        setUiBusy(true);
        m_errorLabel->setText("Connexion...");
        m_errorLabel->show();

        auto future = AuthService::instance().login(username, password);
        auto* watcher = new QFutureWatcher<User>(this);
        connect(watcher, &QFutureWatcher<User>::finished, this, [this, watcher]() {
            const User user = watcher->future().result();
            watcher->deleteLater();
            setUiBusy(false);

            if (!user.id().isEmpty()) {
                m_errorLabel->hide();
                emit loginSuccessful(user.username());
                return;
            }

            const QString msg = AuthService::instance().lastError().isEmpty()
                                    ? QStringLiteral("Login échoué")
                                    : AuthService::instance().lastError();
            m_errorLabel->setText(msg);
            m_errorLabel->show();
        });
        watcher->setFuture(future);
    }

    void setUiBusy(bool busy) {
        m_usernameInput->setEnabled(!busy);
        m_passwordInput->setEnabled(!busy);
        if (m_loginBtn) m_loginBtn->setEnabled(!busy);
    }
    
private:
    QLineEdit* m_usernameInput;
    QLineEdit* m_passwordInput;
    QLabel* m_errorLabel;
    QPushButton* m_loginBtn = nullptr;
};
