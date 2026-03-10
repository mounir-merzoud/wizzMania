#pragma once

#include <QMainWindow>
#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QFrame>
#include <QVector>
#include <QDateTime>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QDebug>
#include <QMessageBox>
#include <QFontMetrics>
#include <QMenu>
#include <QCursor>
#include <QHash>
#include <functional>
#include "../models/Contact.h"
#include "../models/Message.h"
#include "../services/MessagingService.h"
#include "../services/AuthService.h"
#include "CreateUserDialog.h"
#include "CreateRoomDialog.h"
#include "DeleteUserDialog.h"
#include "DeleteRoomDialog.h"
#include "../utils/StyleHelper.h"

// Item affiché dans la sidebar (construit depuis la liste backend des utilisateurs)
struct Conversation {
    QString contactId;
    QString contactName;
    bool hasUnread = false;

    Conversation() = default;
    Conversation(const QString& id, const QString& name)
        : contactId(id), contactName(name) {}
};

class MainWindow : public QMainWindow {
    Q_OBJECT
    
public:
    explicit MainWindow(QWidget* parent = nullptr) : QMainWindow(parent) {
        setWindowTitle("MSF Messenger");
        setMinimumSize(1100, 700);
        
        QWidget* central = new QWidget(this);
        setCentralWidget(central);
        
        QHBoxLayout* mainLayout = new QHBoxLayout(central);
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->setSpacing(0);
        
        // Left sidebar - Conversations list
        m_sidebar = createSidebar();
        m_sidebar->setFixedWidth(400);
        m_sidebarVisible = true;
        
        // Right panel - Messages area
        m_messagesPanel = createMessagesPanel();
        
        mainLayout->addWidget(m_sidebar);
        mainLayout->addWidget(m_messagesPanel, 1);

        // Wire dynamique: contacts/messages viennent du backend via MessagingService
        connect(&MessagingService::instance(), &MessagingService::contactsUpdated, this, [this]() {
            rebuildConversationsFromContacts();
            if (m_errorBanner) {
                m_errorBanner->hide();
                m_errorBanner->clear();
            }
        });
        connect(&MessagingService::instance(), &MessagingService::messagesUpdated, this, [this](const QString& conversationId) {
            if (m_selectedConversation < 0 || m_selectedConversation >= m_conversations.size()) {
                return;
            }
            if (m_conversations[m_selectedConversation].contactId == conversationId) {
                updateMessagesPanel();
            }
            updateConversationsList();
        });
        connect(&MessagingService::instance(), &MessagingService::errorOccurred, this, [](const QString& err) {
            qDebug() << "Messaging error:" << err;
        });

        connect(&MessagingService::instance(), &MessagingService::errorOccurred, this, [this](const QString& err) {
            if (!m_errorBanner) {
                return;
            }
            m_errorBanner->setText(err);
            m_errorBanner->show();
        });

        // Utiliser le cache existant si déjà rempli (login déjà fait)
        rebuildConversationsFromContacts();
        // Et déclencher un refresh réseau
        MessagingService::instance().refreshContacts();
    }
    
private:
    void rebuildConversationsFromContacts() {
        m_conversations.clear();
        const QList<Contact> contacts = MessagingService::instance().getContacts();
        m_conversations.reserve(contacts.size());
        for (const auto& c : contacts) {
            if (c.id().trimmed().isEmpty()) {
                continue;
            }
            m_conversations.append(Conversation(c.id(), c.name()));
        }

        if (m_conversations.isEmpty()) {
            m_selectedConversation = -1;
        } else if (m_selectedConversation < 0 || m_selectedConversation >= m_conversations.size()) {
            m_selectedConversation = 0;
        }

        updateConversationsList();
        if (m_selectedConversation >= 0) {
            updateMessagesPanel();
        }
    }
    
    QWidget* createSidebar() {
        QWidget* sidebar = new QWidget(this);
        sidebar->setStyleSheet("background:" + StyleHelper::white() + ";");
        
        QVBoxLayout* layout = new QVBoxLayout(sidebar);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Header avec menu + search + new chat
        QWidget* header = new QWidget(sidebar);
        header->setFixedHeight(60);
        header->setStyleSheet("background:" + StyleHelper::white() + "; border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(16, 0, 16, 0);
        headerLayout->setSpacing(12);
        
        m_searchBox = new QLineEdit(header);
        m_searchBox->setPlaceholderText("Search");
        m_searchBox->setStyleSheet(
            "QLineEdit {"
            "  background:" + StyleHelper::lightGray() + ";"
            "  border:none;"
            "  border-radius:18px;"
            "  padding:8px 16px;"
            "  font-size:14px;"
            "  color:" + StyleHelper::black() + ";"
            "}"
            "QLineEdit:focus {"
            "  background:#E8E8E8;"
            "}"
        );
        connect(m_searchBox, &QLineEdit::textChanged, this, &MainWindow::onSearchTextChanged);
        
        QPushButton* newChatBtn = new QPushButton("✎", header);
        newChatBtn->setFixedSize(40, 40);
        newChatBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:20px; color:" + StyleHelper::darkGray() + "; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(newChatBtn, &QPushButton::clicked, this, &MainWindow::onNewChatClicked);
        
        headerLayout->addWidget(m_searchBox, 1);
        headerLayout->addWidget(newChatBtn);
        
        layout->addWidget(header);
        
        // Tabs (All Chats, Projects, Important)
        QWidget* tabs = new QWidget(sidebar);
        tabs->setFixedHeight(48);
        tabs->setStyleSheet("background:" + StyleHelper::white() + "; border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* tabsLayout = new QHBoxLayout(tabs);
        tabsLayout->setContentsMargins(16, 0, 16, 0);
        tabsLayout->setSpacing(24);
        
        m_allChatsTab = createTab("All Chats", true);
        m_projectsTab = createTab("Projects", false);
        m_importantTab = createTab("Important", false);
        
        connect(m_allChatsTab, &QPushButton::clicked, [this]() { switchTab(0); });
        connect(m_projectsTab, &QPushButton::clicked, [this]() { switchTab(1); });
        connect(m_importantTab, &QPushButton::clicked, [this]() { switchTab(2); });
        
        tabsLayout->addWidget(m_allChatsTab);
        tabsLayout->addWidget(m_projectsTab);
        tabsLayout->addWidget(m_importantTab);
        tabsLayout->addStretch();
        
        layout->addWidget(tabs);

        // Erreurs backend (évite l'impression de "liste vide" / "mock")
        m_errorBanner = new QLabel(sidebar);
        m_errorBanner->setWordWrap(true);
        m_errorBanner->setStyleSheet(
            "background:#F8D7DA;"
            "color:#842029;"
            "padding:10px 14px;"
            "border-bottom:1px solid " + StyleHelper::borderGray() + ";"
        );
        m_errorBanner->hide();
        layout->addWidget(m_errorBanner);
        
        // Conversations list
        QScrollArea* scrollArea = new QScrollArea(sidebar);
        scrollArea->setWidgetResizable(true);
        scrollArea->setFrameShape(QFrame::NoFrame);
        scrollArea->setStyleSheet("background:" + StyleHelper::white() + "; border:none;");
        
        m_conversationsContainer = new QWidget();
        m_conversationsLayout = new QVBoxLayout(m_conversationsContainer);
        m_conversationsLayout->setContentsMargins(0, 0, 0, 0);
        m_conversationsLayout->setSpacing(0);
        
        // Remplir avec les conversations
        updateConversationsList();
        
        m_conversationsLayout->addStretch();
        scrollArea->setWidget(m_conversationsContainer);
        
        layout->addWidget(scrollArea, 1);
        
        return sidebar;
    }
    
    void updateConversationsList() {
        // Supprimer les anciens widgets
        while (m_conversationsLayout->count() > 1) {
            QLayoutItem* item = m_conversationsLayout->takeAt(0);
            if (item->widget()) {
                item->widget()->deleteLater();
            }
            delete item;
        }
        
        // Ajouter les conversations filtrées
        QString searchText = m_searchBox ? m_searchBox->text().toLower() : "";
        
        for (int i = 0; i < m_conversations.size(); ++i) {
            const auto& conv = m_conversations[i];

            const QList<Message> msgs = MessagingService::instance().getMessages(conv.contactId);
            const QString lastMessage = msgs.isEmpty() ? QStringLiteral("—") : msgs.last().content();
            const QString timeStr = msgs.isEmpty() ? QString() : msgs.last().timestamp().toString("HH:mm");
            
            // Filtrer par recherche
            if (!searchText.isEmpty()) {
                if (!conv.contactName.toLower().contains(searchText) && 
                    !lastMessage.toLower().contains(searchText)) {
                    continue;
                }
            }
            
            QWidget* row = createConversationRow(
                conv.contactName, 
                lastMessage, 
                timeStr, 
                conv.hasUnread,
                i
            );
            m_conversationsLayout->insertWidget(m_conversationsLayout->count() - 1, row);
        }
    }
    
    QPushButton* createTab(const QString& text, bool active) {
        QPushButton* tab = new QPushButton(text);
        tab->setCursor(Qt::PointingHandCursor);
        tab->setFixedHeight(48);
        
        if (active) {
            tab->setStyleSheet(
                "QPushButton {"
                "  background:transparent;"
                "  border:none;"
                "  border-bottom:2px solid " + StyleHelper::primaryRed() + ";"
                "  color:" + StyleHelper::black() + ";"
                "  font-size:14px;"
                "  font-weight:500;"
                "  padding:0 4px;"
                "}"
            );
        } else {
            tab->setStyleSheet(
                "QPushButton {"
                "  background:transparent;"
                "  border:none;"
                "  color:" + StyleHelper::textLight() + ";"
                "  font-size:14px;"
                "  padding:0 4px;"
                "}"
                "QPushButton:hover {"
                "  color:" + StyleHelper::darkGray() + ";"
                "}"
            );
        }
        return tab;
    }
    
    
    QWidget* createConversationRow(const QString& name, const QString& message, const QString& time, bool hasNotification, int conversationIndex) {
        QWidget* row = new QWidget();
        row->setFixedHeight(64);
        row->setCursor(Qt::PointingHandCursor);
        row->setProperty("conversationIndex", conversationIndex);
        
        // Style avec sélection - plus épuré
        QString baseStyle = 
            "QWidget {"
            "  background:" + (m_selectedConversation == conversationIndex ? "#F0F0F0" : StyleHelper::white()) + ";"
            "  border-left:3px solid " + (m_selectedConversation == conversationIndex ? StyleHelper::primaryRed() : "transparent") + ";"
            "}"
            "QWidget:hover {"
            "  background:#F8F8F8;"
            "}";
        row->setStyleSheet(baseStyle);
        
        // Connecter le clic
        row->installEventFilter(new ClickableWidget(this, [this, conversationIndex]() {
            selectConversation(conversationIndex);
        }));
        
        QHBoxLayout* rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(20, 12, 20, 12);
        rowLayout->setSpacing(14);
        
        // Avatar circulaire avec initiales
        QLabel* avatar = new QLabel();
        avatar->setFixedSize(40, 40);
        
        // Extraire initiales du nom
        QString initials;
        QStringList nameParts = name.split(" ");
        if (nameParts.size() >= 2) {
            initials = QString(nameParts[0][0]) + QString(nameParts[1][0]);
        } else if (!nameParts.isEmpty()) {
            initials = QString(nameParts[0][0]);
        }
        
        avatar->setText(initials);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setStyleSheet(
            "background:" + StyleHelper::primaryRed() + ";"
            "color:white;"
            "border-radius:20px;"
            "font-size:14px;"
            "font-weight:600;"
        );
        
        // Bloc unique pour nom + message + time
        QWidget* infoBlock = new QWidget();
        QVBoxLayout* infoLayout = new QVBoxLayout(infoBlock);
        infoLayout->setContentsMargins(0, 0, 0, 0);
        infoLayout->setSpacing(2);
        
        // Ligne 1: Nom + Time
        QWidget* topLine = new QWidget();
        QHBoxLayout* topLayout = new QHBoxLayout(topLine);
        topLayout->setContentsMargins(0, 0, 0, 0);
        topLayout->setSpacing(8);
        
        QLabel* nameLabel = new QLabel(name);
        nameLabel->setStyleSheet("font-size:14px; font-weight:600; color:" + StyleHelper::black() + ";");
        
        QLabel* timeLabel = new QLabel(time);
        timeLabel->setStyleSheet("font-size:11px; color:" + StyleHelper::textLight() + ";");
        
        topLayout->addWidget(nameLabel);
        topLayout->addStretch();
        topLayout->addWidget(timeLabel);
        
        // Ligne 2: Message + Badge
        QWidget* bottomLine = new QWidget();
        QHBoxLayout* bottomLayout = new QHBoxLayout(bottomLine);
        bottomLayout->setContentsMargins(0, 0, 0, 0);
        bottomLayout->setSpacing(8);
        
        QLabel* messageLabel = new QLabel(message);
        messageLabel->setStyleSheet("font-size:13px; color:" + StyleHelper::textLight() + ";");
        messageLabel->setWordWrap(false);
        messageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        
        QFontMetrics metrics(messageLabel->font());
        QString elidedText = metrics.elidedText(message, Qt::ElideRight, 250);
        messageLabel->setText(elidedText);
        
        bottomLayout->addWidget(messageLabel, 1);
        
        if (hasNotification) {
            QLabel* badge = new QLabel("1");
            badge->setFixedSize(18, 18);
            badge->setAlignment(Qt::AlignCenter);
            badge->setStyleSheet(
                "background:" + StyleHelper::primaryRed() + ";"
                "color:white;"
                "border-radius:9px;"
                "font-size:10px;"
                "font-weight:600;"
            );
            bottomLayout->addWidget(badge);
        }
        
        infoLayout->addWidget(topLine);
        infoLayout->addWidget(bottomLine);
        
        rowLayout->addWidget(avatar);
        rowLayout->addWidget(infoBlock, 1);
        
        return row;
    }
    
    // Helper class pour rendre les widgets cliquables
    class ClickableWidget : public QObject {
    public:
        ClickableWidget(QObject* parent, std::function<void()> callback)
            : QObject(parent), m_callback(callback) {}
        
    protected:
        bool eventFilter(QObject* obj, QEvent* event) override {
            if (event->type() == QEvent::MouseButtonPress) {
                m_callback();
                return true;
            }
            return QObject::eventFilter(obj, event);
        }
        
    private:
        std::function<void()> m_callback;
    };
    
    QWidget* createMessagesPanel() {
        QWidget* panel = new QWidget();
        panel->setStyleSheet("background:" + StyleHelper::lightGray() + ";");
        
        QVBoxLayout* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Header
        QWidget* header = new QWidget();
        header->setObjectName("messagesHeader");
        header->setFixedHeight(60);
        header->setStyleSheet("background:" + StyleHelper::white() + "; border-bottom:1px solid " + StyleHelper::borderGray() + ";");
        
        QHBoxLayout* headerLayout = new QHBoxLayout(header);
        headerLayout->setContentsMargins(24, 0, 24, 0);
        headerLayout->setSpacing(12);
        
        // Bouton menu burger (toujours visible)
        QPushButton* menuBtn = new QPushButton("☰");
        menuBtn->setFixedSize(40, 40);
        menuBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:20px; color:" + StyleHelper::darkGray() + "; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(menuBtn, &QPushButton::clicked, this, &MainWindow::onMenuClicked);
        
        QLabel* avatar = new QLabel();
        avatar->setFixedSize(40, 40);
        avatar->setStyleSheet("background:#C0C0C0; border-radius:20px;");
        
        QLabel* contactName = new QLabel("Select a conversation");
        contactName->setObjectName("contactNameLabel");
        contactName->setStyleSheet("font-size:20px; font-weight:600; color:" + StyleHelper::black() + ";");
        
        headerLayout->addWidget(menuBtn);
        headerLayout->addWidget(avatar);
        headerLayout->addWidget(contactName);
        headerLayout->addStretch();
        
        QPushButton* searchBtn = new QPushButton("🔍");
        searchBtn->setFixedSize(36, 36);
        searchBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:18px; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(searchBtn, &QPushButton::clicked, this, &MainWindow::onSearchInConversationClicked);
        
        QPushButton* moreBtn = new QPushButton("⋯");
        moreBtn->setFixedSize(36, 36);
        moreBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:20px; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(moreBtn, &QPushButton::clicked, this, &MainWindow::onMoreOptionsClicked);
        
        headerLayout->addWidget(searchBtn);
        headerLayout->addWidget(moreBtn);
        
        // Messages area
        QScrollArea* messagesArea = new QScrollArea();
        messagesArea->setObjectName("messagesScrollArea");
        messagesArea->setWidgetResizable(true);
        messagesArea->setFrameShape(QFrame::NoFrame);
        messagesArea->setStyleSheet("background:" + StyleHelper::lightGray() + "; border:none;");
        
        QWidget* messagesContainer = new QWidget();
        QVBoxLayout* messagesLayout = new QVBoxLayout(messagesContainer);
        messagesLayout->setContentsMargins(24, 16, 24, 16);
        messagesLayout->setSpacing(12);
        messagesLayout->setAlignment(Qt::AlignTop);
        
        QLabel* emptyLabel = new QLabel("Select a conversation to start messaging");
        emptyLabel->setAlignment(Qt::AlignCenter);
        emptyLabel->setStyleSheet("font-size:14px; color:" + StyleHelper::textLight() + "; padding:40px;");
        messagesLayout->addWidget(emptyLabel);
        
        messagesLayout->addStretch();
        messagesArea->setWidget(messagesContainer);
        
        // Input bar
        QWidget* inputBar = new QWidget();
        inputBar->setFixedHeight(64);
        inputBar->setStyleSheet("background:" + StyleHelper::white() + ";");
        
        QHBoxLayout* inputLayout = new QHBoxLayout(inputBar);
        inputLayout->setContentsMargins(16, 12, 16, 12);
        inputLayout->setSpacing(12);
        
        QPushButton* attachBtn = new QPushButton("📎");
        attachBtn->setFixedSize(40, 40);
        attachBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:20px; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(attachBtn, &QPushButton::clicked, this, &MainWindow::onAttachFileClicked);
        
        m_messageInput = new QLineEdit();
        m_messageInput->setPlaceholderText("Message");
        m_messageInput->setStyleSheet(
            "QLineEdit {"
            "  background:" + StyleHelper::lightGray() + ";"
            "  border:none;"
            "  border-radius:20px;"
            "  padding:10px 16px;"
            "  font-size:14px;"
            "  color:" + StyleHelper::black() + ";"
            "}"
            "QLineEdit:focus {"
            "  background:#E8E8E8;"
            "}"
        );
        connect(m_messageInput, &QLineEdit::returnPressed, this, &MainWindow::onSendMessage);
        
        QPushButton* emojiBtn = new QPushButton("☺");
        emojiBtn->setFixedSize(40, 40);
        emojiBtn->setStyleSheet(
            "QPushButton { background:transparent; border:none; font-size:20px; color:" + StyleHelper::darkGray() + "; border-radius:8px; }"
            "QPushButton:hover { background:rgba(0,0,0,0.03); }"
            "QPushButton:pressed { background:rgba(0,0,0,0.08); }"
        );
        connect(emojiBtn, &QPushButton::clicked, this, &MainWindow::onEmojiClicked);
        
        m_sendBtn = new QPushButton("➤");
        m_sendBtn->setFixedSize(40, 40);
        m_sendBtn->setStyleSheet(
            "QPushButton { background:" + StyleHelper::primaryRed() + "; border:none; font-size:18px; color:white; border-radius:20px; }"
            "QPushButton:hover { background:#C20016; }"
            "QPushButton:pressed { background:#A50013; }"
        );
        connect(m_sendBtn, &QPushButton::clicked, this, &MainWindow::onSendMessage);
        
        inputLayout->addWidget(attachBtn);
        inputLayout->addWidget(m_messageInput, 1);
        inputLayout->addWidget(emojiBtn);
        inputLayout->addWidget(m_sendBtn);
        
        layout->addWidget(header);
        layout->addWidget(messagesArea, 1);
        layout->addWidget(inputBar);
        
        return panel;
    }
    
    static QString formatMessageTimestamp(const QDateTime& ts) {
        if (!ts.isValid()) {
            return QString();
        }

        const QDate today = QDate::currentDate();
        if (ts.date() == today) {
            return ts.toString("HH:mm");
        }
        return ts.toString("dd/MM HH:mm");
    }

    QWidget* createMessageListItem(const QString& sender,
                                  const QString& text,
                                  const QString& time,
                                  bool showHeader) {
        QWidget* root = new QWidget();
        QVBoxLayout* rootLayout = new QVBoxLayout(root);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->setSpacing(4);

        if (showHeader) {
            QWidget* header = new QWidget(root);
            QHBoxLayout* headerLayout = new QHBoxLayout(header);
            headerLayout->setContentsMargins(0, 0, 0, 0);
            headerLayout->setSpacing(8);

            QLabel* senderLabel = new QLabel(sender);
            senderLabel->setStyleSheet(
                "font-size:13px;"
                "font-weight:600;"
                "color:" + StyleHelper::darkGray() + ";"
            );
            QLabel* timeLabel = new QLabel(time);
            timeLabel->setStyleSheet(
                "font-size:12px;"
                "color:" + StyleHelper::textLight() + ";"
            );

            headerLayout->addWidget(senderLabel);
            headerLayout->addStretch();
            headerLayout->addWidget(timeLabel);

            rootLayout->addWidget(header);
        }

        QLabel* textLabel = new QLabel(text);
        textLabel->setWordWrap(true);
        textLabel->setStyleSheet(
            "font-size:14px;"
            "color:" + StyleHelper::black() + ";"
        );
        rootLayout->addWidget(textLabel);

        if (!showHeader) {
            QLabel* timeLabel = new QLabel(time);
            timeLabel->setAlignment(Qt::AlignRight);
            timeLabel->setStyleSheet(
                "font-size:12px;"
                "color:" + StyleHelper::textLight() + ";"
            );
            rootLayout->addWidget(timeLabel);
        }

        return root;
    }
    
    QWidget* createVoiceMessageBubble(const QString& time) {
        QWidget* container = new QWidget();
        QHBoxLayout* containerLayout = new QHBoxLayout(container);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->addStretch();
        
        QWidget* bubble = new QWidget();
        bubble->setFixedSize(200, 200);
        bubble->setStyleSheet(
            "background:" + StyleHelper::bubbleSent() + ";"
            "border-radius:100px;"
        );
        
        QVBoxLayout* bubbleLayout = new QVBoxLayout(bubble);
        bubbleLayout->setAlignment(Qt::AlignCenter);
        
        QLabel* icon = new QLabel("🔇");
        icon->setAlignment(Qt::AlignCenter);
        icon->setStyleSheet("font-size:32px;");
        bubbleLayout->addWidget(icon);
        
        QLabel* timeLabel = new QLabel(time);
        timeLabel->setStyleSheet("font-size:11px; color:rgba(255,255,255,0.7);");
        timeLabel->setAlignment(Qt::AlignCenter);
        
        QVBoxLayout* mainLayout = new QVBoxLayout();
        mainLayout->setContentsMargins(0, 0, 0, 0);
        mainLayout->addWidget(bubble);
        mainLayout->addWidget(timeLabel);
        
        QWidget* wrapper = new QWidget();
        wrapper->setLayout(mainLayout);
        
        containerLayout->addWidget(wrapper);
        
        return container;
    }
    
    void selectConversation(int index) {
        if (index < 0 || index >= m_conversations.size()) return;
        
        m_selectedConversation = index;

        // Charger l'historique depuis le backend (async)
        const QString cid = m_conversations[m_selectedConversation].contactId;
        if (!cid.trimmed().isEmpty()) {
            MessagingService::instance().refreshMessages(cid);
        }
        
        // Rafraîchir la liste pour afficher la sélection
        updateConversationsList();
        
        // Mettre à jour le panneau de messages
        updateMessagesPanel();
    }
    
    void updateMessagesPanel() {
        if (m_selectedConversation < 0 || m_selectedConversation >= m_conversations.size()) {
            return;
        }
        
        const Conversation& conv = m_conversations[m_selectedConversation];
        
        // Mettre à jour le header avec le nom du contact
        QWidget* header = m_messagesPanel->findChild<QWidget*>("messagesHeader");
        if (header) {
            QLabel* contactName = header->findChild<QLabel*>("contactNameLabel");
            if (contactName) {
                contactName->setText(conv.contactName);
            }
        }
        
        // Nettoyer les anciens messages
        QScrollArea* scrollArea = m_messagesPanel->findChild<QScrollArea*>("messagesScrollArea");
        if (!scrollArea) return;
        
        QWidget* messagesContainer = new QWidget();
        QVBoxLayout* messagesLayout = new QVBoxLayout(messagesContainer);
        messagesLayout->setContentsMargins(20, 20, 20, 20);
        messagesLayout->setSpacing(12);
        messagesLayout->setAlignment(Qt::AlignTop);
        
        // Build id -> name mapping from cached contacts (for room messages)
        QHash<QString, QString> idToName;
        {
            const QList<Contact> contacts = MessagingService::instance().getContacts();
            idToName.reserve(contacts.size());
            for (const auto& c : contacts) {
                if (!c.id().trimmed().isEmpty() && !c.name().trimmed().isEmpty()) {
                    idToName.insert(c.id(), c.name());
                }
            }
        }

        // Ajouter tous les messages de la conversation (backend cache)
        const QList<Message> messages = MessagingService::instance().getMessages(conv.contactId);
        QString lastSenderKey;
        QDate lastDate;
        for (const Message& msg : messages) {
            const QString timeStr = formatMessageTimestamp(msg.timestamp());

            // Sender key: for Sent, force a stable key; for Received, use senderId if present.
            const bool isSent = (msg.type() == Message::Type::Sent);
            QString senderKey = isSent ? QStringLiteral("__me__") : msg.senderId().trimmed();
            if (!isSent && senderKey.isEmpty()) {
                // Fallback for backward-compat when senderId isn't provided.
                senderKey = QStringLiteral("__other__");
            }

            QString senderName;
            if (isSent) {
                senderName = QStringLiteral("Vous");
            } else {
                // For DMs, the conversation header already is the contact name; still show it.
                senderName = idToName.value(senderKey, conv.contactName);
            }

            const QDate msgDate = msg.timestamp().date();
            const bool showHeader = (senderKey != lastSenderKey) || (!lastDate.isValid() || msgDate != lastDate);

            messagesLayout->addWidget(createMessageListItem(
                senderName,
                msg.content(),
                timeStr,
                showHeader
            ));

            lastSenderKey = senderKey;
            lastDate = msgDate;
        }
        
        messagesLayout->addStretch();
        scrollArea->setWidget(messagesContainer);
        
        // Scroller vers le bas
        QTimer::singleShot(100, [scrollArea]() {
            scrollArea->verticalScrollBar()->setValue(scrollArea->verticalScrollBar()->maximum());
        });
    }
    
    void onSearchTextChanged() {
        updateConversationsList();
    }
    
    void switchTab(int tabIndex) {
        // Mettre à jour les styles des onglets
        QString activeStyle = 
            "QPushButton { background:transparent; color:" + StyleHelper::primaryRed() + "; font-weight:600; padding:8px 16px; border:none; border-bottom:2px solid " + StyleHelper::primaryRed() + "; }"
            "QPushButton:hover { background:" + StyleHelper::lightGray() + "; }";
        
        QString inactiveStyle = 
            "QPushButton { background:transparent; color:" + StyleHelper::textLight() + "; font-weight:500; padding:8px 16px; border:none; border-bottom:2px solid transparent; }"
            "QPushButton:hover { background:" + StyleHelper::lightGray() + "; }";
        
        m_allChatsTab->setStyleSheet(tabIndex == 0 ? activeStyle : inactiveStyle);
        m_projectsTab->setStyleSheet(tabIndex == 1 ? activeStyle : inactiveStyle);
        m_importantTab->setStyleSheet(tabIndex == 2 ? activeStyle : inactiveStyle);
        
        m_currentTab = tabIndex;
        
        // Mettre à jour la liste (on peut filtrer par catégorie plus tard)
        updateConversationsList();
    }
    
    // Handlers pour les boutons
    void onMenuClicked() {
        m_sidebarVisible = !m_sidebarVisible;
        
        if (m_sidebarVisible) {
            m_sidebar->setFixedWidth(400);
            m_sidebar->show();
        } else {
            m_sidebar->setFixedWidth(0);
            m_sidebar->hide();
        }
        
        qDebug() << "Menu clicked - Sidebar" << (m_sidebarVisible ? "visible" : "hidden");
    }
    
    void onNewChatClicked() {
        if (AuthService::instance().isAdmin()) {
            QMenu menu(this);
            QAction* createUser = menu.addAction(QStringLiteral("Créer utilisateur"));
            QAction* createRoom = menu.addAction(QStringLiteral("Créer salon"));
            menu.addSeparator();
            QAction* deleteUser = menu.addAction(QStringLiteral("Supprimer utilisateur"));
            QAction* deleteRoom = menu.addAction(QStringLiteral("Supprimer salon"));

            QAction* picked = menu.exec(QCursor::pos());
            if (!picked) {
                return;
            }

            if (picked == createUser) {
                CreateUserDialog dlg(this);
                connect(&dlg, &CreateUserDialog::userCreated, this, []() {
                    MessagingService::instance().refreshContacts();
                });
                dlg.exec();
                return;
            }

            if (picked == createRoom) {
                CreateRoomDialog dlg(this);
                connect(&dlg, &CreateRoomDialog::roomCreated, this, []() {
                    MessagingService::instance().refreshContacts();
                });
                dlg.exec();
                return;
            }

            if (picked == deleteUser) {
                DeleteUserDialog dlg(this);
                connect(&dlg, &DeleteUserDialog::userDeleted, this, []() {
                    MessagingService::instance().refreshContacts();
                });
                dlg.exec();
                return;
            }

            if (picked == deleteRoom) {
                DeleteRoomDialog dlg(this);
                connect(&dlg, &DeleteRoomDialog::roomDeleted, this, []() {
                    MessagingService::instance().refreshContacts();
                });
                dlg.exec();
                return;
            }

            return;
        }

        QMessageBox::information(this,
                                 QStringLiteral("Info"),
                                 QStringLiteral("Création d'utilisateur réservée aux administrateurs."));
    }
    
    void onSearchInConversationClicked() {
        // TODO: Activer recherche dans la conversation actuelle
        qDebug() << "Search in conversation clicked - À implémenter";
    }
    
    void onMoreOptionsClicked() {
        // TODO: Menu contextuel (Mute, Archive, Delete)
        qDebug() << "More options clicked - À implémenter";
    }
    
    void onAttachFileClicked() {
        // TODO: Ouvrir sélecteur de fichiers
        qDebug() << "Attach file clicked - À implémenter";
    }
    
    void onEmojiClicked() {
        if (!m_messageInput) {
            return;
        }

        QMenu menu(this);
        menu.setStyleSheet(
            "QMenu {"
            "  background:" + StyleHelper::white() + ";"
            "  border:1px solid " + StyleHelper::borderGray() + ";"
            "  padding:6px;"
            "}"
            "QMenu::item {"
            "  padding:8px 10px;"
            "  border-radius:8px;"
            "  color:" + StyleHelper::black() + ";"
            "}"
            "QMenu::item:selected {"
            "  background:" + StyleHelper::lightGray() + ";"
            "}"
        );

        const QStringList emojis{
            QStringLiteral("😀"), QStringLiteral("😁"), QStringLiteral("😂"), QStringLiteral("🙂"),
            QStringLiteral("😉"), QStringLiteral("😍"), QStringLiteral("😎"), QStringLiteral("🤔"),
            QStringLiteral("😢"), QStringLiteral("😡"), QStringLiteral("👍"), QStringLiteral("👎"),
            QStringLiteral("🙏"), QStringLiteral("👏"), QStringLiteral("🎉"), QStringLiteral("❤️")
        };

        QAction* picked = nullptr;
        for (const auto& e : emojis) {
            QAction* a = menu.addAction(e);
            a->setData(e);
        }

        picked = menu.exec(QCursor::pos());
        if (!picked) {
            return;
        }

        const QString e = picked->data().toString();
        if (e.isEmpty()) {
            return;
        }

        // Insert at cursor.
        m_messageInput->insert(e);
        m_messageInput->setFocus();
    }
    
    void onSendMessage() {
        if (!m_messageInput || m_messageInput->text().trimmed().isEmpty()) {
            return;
        }
        
        if (m_selectedConversation < 0 || m_selectedConversation >= m_conversations.size()) {
            return;
        }
        
        QString messageText = m_messageInput->text().trimmed();

        // Vider l'input
        m_messageInput->clear();

        const QString cid = m_conversations[m_selectedConversation].contactId;
        if (!cid.trimmed().isEmpty()) {
            MessagingService::instance().sendMessage(cid, messageText);
        }
    }
    
    // Variables membres
    QVector<Conversation> m_conversations;
    QWidget* m_sidebar;
    QWidget* m_messagesPanel;
    QWidget* m_conversationsContainer;
    QLabel* m_errorBanner = nullptr;
    QLineEdit* m_searchBox;
    QLineEdit* m_messageInput;
    QVBoxLayout* m_conversationsLayout;
    QPushButton* m_allChatsTab;
    QPushButton* m_projectsTab;
    QPushButton* m_importantTab;
    QPushButton* m_sendBtn;
    int m_selectedConversation = -1;
    int m_currentTab = 0;
    bool m_sidebarVisible = true;
    
signals:
    void logoutRequested();
};
