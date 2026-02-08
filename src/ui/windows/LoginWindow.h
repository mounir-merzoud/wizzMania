#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QMessageBox>
#include "../utils/StyleHelper.h"

class LoginWindow : public QWidget {
    Q_OBJECT
    
public:
    explicit LoginWindow(QWidget* parent = nullptr) : QWidget(parent) {
        setWindowTitle("WizzMania - Login");
        setFixedSize(500, 650);
        
        setStyleSheet("QWidget { background:" + StyleHelper::blueBg() + "; }");
        
        QVBoxLayout* mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(50, 80, 50, 80);
        mainLayout->setSpacing(24);
        
        // Logo & Title
        // QLabel* logo = new QLabel("🧙‍♂️", this);
        // logo->setAlignment(Qt::AlignCenter);
        // logo->setStyleSheet("font-size:64px;");
        
        QLabel* title = new QLabel("WizzMania", this);
        title->setAlignment(Qt::AlignCenter);
        title->setStyleSheet(
            "font-size:32px;"
            "font-weight:700;"
            "color:" + StyleHelper::darkText() + ";"
        );
        
        QLabel* subtitle = new QLabel("Sign in to continue", this);
        subtitle->setAlignment(Qt::AlignCenter);
        subtitle->setStyleSheet("font-size:15px;color:#6b7280;");
        
        mainLayout->addWidget(logo);
        mainLayout->addWidget(title);
        mainLayout->addWidget(subtitle);
        mainLayout->addSpacing(20);
        
        // Login form
        QWidget* formCard = new QWidget(this);
        formCard->setStyleSheet(StyleHelper::cardStyle());
        QVBoxLayout* formLayout = new QVBoxLayout(formCard);
        formLayout->setSpacing(16);
        
        m_usernameInput = new QLineEdit(formCard);
        m_usernameInput->setPlaceholderText("Username");
        m_usernameInput->setStyleSheet(StyleHelper::inputStyle());
        
        m_passwordInput = new QLineEdit(formCard);
        m_passwordInput->setPlaceholderText("Password");
        m_passwordInput->setEchoMode(QLineEdit::Password);
        m_passwordInput->setStyleSheet(StyleHelper::inputStyle());
        
        QPushButton* loginBtn = new QPushButton("Login", formCard);
        loginBtn->setStyleSheet(StyleHelper::buttonStyle());
        loginBtn->setCursor(Qt::PointingHandCursor);
        
        connect(loginBtn, &QPushButton::clicked, this, &LoginWindow::onLoginClicked);
        
        // Error label
        m_errorLabel = new QLabel(formCard);
        m_errorLabel->setStyleSheet("color:" + StyleHelper::redAlert() + ";font-size:14px;");
        m_errorLabel->setAlignment(Qt::AlignCenter);
        m_errorLabel->setWordWrap(true);
        m_errorLabel->hide();
        
        formLayout->addWidget(m_usernameInput);
        formLayout->addWidget(m_passwordInput);
        formLayout->addWidget(m_errorLabel);
        formLayout->addWidget(loginBtn);
        
        mainLayout->addWidget(formCard);
        mainLayout->addStretch();
    }
    
signals:
    void loginSuccessful(const QString& username);
    
private slots:
    void onLoginClicked() {
        QString username = m_usernameInput->text().trimmed();
        QString password = m_passwordInput->text();
        
        m_errorLabel->hide();
        
        if (username.isEmpty() || password.isEmpty()) {
            m_errorLabel->setText("Please fill in all fields");
            m_errorLabel->show();
            return;
        }
        
        // TODO: Connect to AuthService
        // For now, accept any non-empty credentials
        if (username == "demo" && password == "demo") {
            emit loginSuccessful(username);
        } else {
            m_errorLabel->setText("Invalid credentials. Try demo/demo");
            m_errorLabel->show();
        }
    }
    
private:
    QLineEdit* m_usernameInput;
    QLineEdit* m_passwordInput;
    QLabel* m_errorLabel;
};
