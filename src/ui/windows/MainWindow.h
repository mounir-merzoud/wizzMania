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
        setWindowTitle("WizzMania");
        setMinimumSize(1200, 800);
        
        // Central widget
        QWidget* central = new QWidget(this);
        setCentralWidget(central);
        
        QHBoxLayout* mainLayout = new QHBoxLayout(central);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Sidebar
        QWidget* sidebar = createSidebar();
        sidebar->setFixedWidth(80);
        
        // Content area with stacked pages
        m_stackedWidget = new QStackedWidget(this);
        m_stackedWidget->setStyleSheet("background:" + StyleHelper::blueBg() + ";");
        
        // Add pages
        m_mapPage = new MapPage(m_stackedWidget);
        m_contactsPage = new ContactsPage(m_stackedWidget);
        m_conversationPage = new ConversationPage(m_stackedWidget);
        
        m_stackedWidget->addWidget(m_mapPage);       // index 0
        m_stackedWidget->addWidget(m_contactsPage);  // index 1
        m_stackedWidget->addWidget(m_conversationPage); // index 2
        
        // Connect contact selection
        connect(m_contactsPage, &ContactsPage::contactClicked, 
                this, &MainWindow::onContactSelected);
        
        mainLayout->addWidget(sidebar);
        mainLayout->addWidget(m_stackedWidget, 1);
        
        // Show map by default
        m_stackedWidget->setCurrentIndex(0);
    }
    
private:
    QWidget* createSidebar() {
        QWidget* sidebar = new QWidget(this);
        sidebar->setStyleSheet(
            "QWidget { background:rgba(255,255,255,0.92); }"
        );
        
        QVBoxLayout* layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(0, 20, 0, 20);
        layout->setSpacing(12);
        
        // Logo
        QLabel* logo = new QLabel("🧙‍♂️", sidebar);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet("font-size:32px;background:transparent;");
        
        layout->addWidget(logo);
        layout->addSpacing(20);
        
        // Navigation buttons
        QPushButton* mapBtn = createNavButton("🗺️", "Map");
        QPushButton* contactsBtn = createNavButton("👥", "Contacts");
        QPushButton* chatsBtn = createNavButton("💬", "Chats");
        QPushButton* filesBtn = createNavButton("📁", "Files");
        
        connect(mapBtn, &QPushButton::clicked, [this]() { 
            m_stackedWidget->setCurrentIndex(0); 
        });
        connect(contactsBtn, &QPushButton::clicked, [this]() { 
            m_stackedWidget->setCurrentIndex(1); 
        });
        connect(chatsBtn, &QPushButton::clicked, [this]() { 
            m_stackedWidget->setCurrentIndex(2); 
        });
        
        layout->addWidget(mapBtn);
        layout->addWidget(contactsBtn);
        layout->addWidget(chatsBtn);
        layout->addWidget(filesBtn);
        layout->addStretch();
        
        // Logout button at bottom
        QPushButton* logoutBtn = createNavButton("🚪", "Logout");
        logoutBtn->setStyleSheet(
            "QPushButton {"
            "  background:transparent;"
            "  border:none;"
            "  color:" + StyleHelper::redAlert() + ";"
            "  font-size:24px;"
            "  padding:12px;"
            "}"
            "QPushButton:hover {"
            "  background:rgba(255,59,48,0.1);"
            "  border-radius:12px;"
            "}"
        );
        connect(logoutBtn, &QPushButton::clicked, this, &MainWindow::logoutRequested);
        
        layout->addWidget(logoutBtn);
        
        return sidebar;
    }
    
    QPushButton* createNavButton(const QString& emoji, const QString& tooltip) {
        QPushButton* btn = new QPushButton(emoji, this);
        btn->setToolTip(tooltip);
        btn->setFixedSize(56, 56);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setStyleSheet(
            "QPushButton {"
            "  background:transparent;"
            "  border:none;"
            "  font-size:24px;"
            "  padding:8px;"
            "}"
            "QPushButton:hover {"
            "  background:rgba(0,122,255,0.1);"
            "  border-radius:12px;"
            "}"
        );
        return btn;
    }
    
    void onContactSelected(const Contact& contact) {
        m_conversationPage->setContact(contact);
        m_stackedWidget->setCurrentIndex(2);
    }
    
signals:
    void logoutRequested();
    
private:
    QStackedWidget* m_stackedWidget;
    MapPage* m_mapPage;
    ContactsPage* m_contactsPage;
    ConversationPage* m_conversationPage;
};
