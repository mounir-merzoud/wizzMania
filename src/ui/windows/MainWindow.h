#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include "../pages/MapPage.h"
#include "../pages/ContactsPage.h"
#include "../pages/ConversationPage.h"
#include "../models/Contact.h"
#include "../utils/StyleHelper.h"

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("MSF Messenger");
        setMinimumSize(1100, 700);
        
        // Central widget
        QWidget* central = new QWidget(this);
        setCentralWidget(central);
        central->setStyleSheet("background:" + StyleHelper::lightGray() + ";");
        
        QHBoxLayout* mainLayout = new QHBoxLayout(central);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Sidebar style WhatsApp
        QWidget* sidebar = createSidebar();
        sidebar->setFixedWidth(70);
        
        // Content area
        m_stackedWidget = new QStackedWidget(this);
        m_stackedWidget->setStyleSheet("background:" + StyleHelper::white() + ";");
        
        // Add pages
        m_contactsPage = new ContactsPage(m_stackedWidget);
        m_conversationPage = new ConversationPage(m_stackedWidget);
        
        m_stackedWidget->addWidget(m_contactsPage);      // index 0
        m_stackedWidget->addWidget(m_conversationPage);  // index 1
        
        // Connect contact selection
        connect(m_contactsPage, &ContactsPage::contactClicked, 
                this, &MainWindow::onContactSelected);
        
        mainLayout->addWidget(sidebar);
        mainLayout->addWidget(m_stackedWidget, 1);
        
        // Show contacts by default
        m_stackedWidget->setCurrentIndex(0);
    }
    
private:
    QWidget* createSidebar() {
        QWidget* sidebar = new QWidget(this);
        sidebar->setStyleSheet(
            "QWidget { "
            "  background:" + StyleHelper::white() + ";"
            "  border-right:1px solid " + StyleHelper::borderGray() + ";"
            "}"
        );
        
        QVBoxLayout* layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(0, 16, 0, 16);
        layout->setSpacing(8);
        
        // Logo MSF en haut
        QLabel* logo = new QLabel("MSF", sidebar);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet(
            "font-size:18px;"
            "font-weight:700;"
            "color:" + StyleHelper::primaryBlue() + ";"
            "padding:12px 0;"
            "background:transparent;"
        );
        
        layout->addWidget(logo);
        layout->addSpacing(24);
        
        // Navigation buttons - icônes simples (texte pour l'instant)
        QPushButton* chatsBtn = createNavButton("💬");
        QPushButton* settingsBtn = createNavButton("⚙️");
        
        connect(chatsBtn, &QPushButton::clicked, [this]() { 
            m_stackedWidget->setCurrentIndex(0); 
        });
        
        layout->addWidget(chatsBtn);
        layout->addStretch();
        
        // Settings et logout en bas
        layout->addWidget(settingsBtn);
        
        QPushButton* logoutBtn = createNavButton("🚪");
        connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::logoutRequested);
        layout->addWidget(logoutBtn);
        
        return sidebar;
    }
    
    QPushButton* createNavButton(const QString& icon) {
        QPushButton* btn = new QPushButton(icon, this);
        btn->setFixedSize(54, 54);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background:transparent;"
            "  border:none;"
            "  font-size:24px;"
            "  border-radius:12px;"
            "}"
            "QPushButton:hover {"
            "  background:" + StyleHelper::lightBlue() + ";"
            "}"
            "QPushButton:pressed {"
            "  background:" + StyleHelper::borderGray() + ";"
            "}"
        );
        return btn;
    }
    
    void onContactSelected(const Contact& contact) {
        m_conversationPage->setContact(contact);
        m_stackedWidget->setCurrentIndex(1);
    }
    
signals:
    void logoutRequested();
    
private:
    QStackedWidget* m_stackedWidget;
    ContactsPage* m_contactsPage;
    ConversationPage* m_conversationPage;
};
