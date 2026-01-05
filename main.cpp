#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QFrame>
#include <QMouseEvent>
#include <QDebug>

// ---------- Colors ----------

static QString kBlueBg()      { return "#a7c7ff"; }
static QString kRedAlert()    { return "#ff3b30"; }
static QString kPrimaryBlue() { return "#007aff"; }

// ---------- PAGES FOR MAIN WINDOW ----------

// MapPage: placeholder for the future map widget
class MapPage : public QWidget {
public:
    MapPage(QWidget *parent=nullptr) : QWidget(parent) {
        QVBoxLayout *lay = new QVBoxLayout(this);
        lay->setContentsMargins(40,40,40,40);
        lay->setSpacing(16);

        QLabel *title = new QLabel("Map", this);
        title->setStyleSheet("font-size:24px;font-weight:700;color:#0b1120;");

        QLabel *placeholder = new QLabel(
            "Here I will add the map widget later.",
            this
        );
        placeholder->setWordWrap(true);
        placeholder->setAlignment(Qt::AlignCenter);
        placeholder->setStyleSheet(
            "background:rgba(255,255,255,0.96);"
            "border-radius:18px;"
            "padding:60px;font-size:16px;color:#111827;"
        );

        lay->addWidget(title);
        lay->addSpacing(8);
        lay->addWidget(placeholder, 1);
    }
};

// ConversationPage: simple fake conversation for a contact
class ConversationPage : public QWidget {
public:
    QLabel *headerName;

    ConversationPage(QWidget *parent=nullptr) : QWidget(parent) {
        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(40,32,40,32);
        root->setSpacing(16);

        headerName = new QLabel("Conversation", this);
        headerName->setStyleSheet("font-size:20px;font-weight:600;color:#0b1120;");

        QLabel *bubble1 = new QLabel("Hi, how can I help you?", this);
        bubble1->setWordWrap(true);
        bubble1->setStyleSheet(
            "background:white;"
            "border-radius:14px;"
            "padding:10px 14px;"
            "max-width:260px;"
        );

        QLabel *bubble2 = new QLabel("Can you Share with me that file.", this);
        bubble2->setWordWrap(true);
        bubble2->setStyleSheet(
            "background:#2563eb;"
            "color:white;"
            "border-radius:14px;"
            "padding:10px 14px;"
            "max-width:260px;"
        );

        QHBoxLayout *row1 = new QHBoxLayout;
        row1->addWidget(bubble1, 0, Qt::AlignLeft);
        row1->addStretch();

        QHBoxLayout *row2 = new QHBoxLayout;
        row2->addStretch();
        row2->addWidget(bubble2, 0, Qt::AlignRight);

        QVBoxLayout *convLayout = new QVBoxLayout;
        convLayout->setSpacing(8);
        convLayout->addLayout(row1);
        convLayout->addLayout(row2);
        convLayout->addStretch();

        QWidget *bottom = new QWidget(this);
        bottom->setStyleSheet(
            "background:rgba(255,255,255,0.9);"
            "border-radius:20px;"
        );
        QHBoxLayout *bottomLay = new QHBoxLayout(bottom);
        bottomLay->setContentsMargins(12,6,12,6);

        QLineEdit *input = new QLineEdit(bottom);
        input->setPlaceholderText("Type a message…");
        input->setStyleSheet("border:none;background:transparent;font-size:13px;");

        QPushButton *send = new QPushButton("Send", bottom);
        send->setStyleSheet(
            "background:" + kPrimaryBlue() + ";"
            "color:white;border-radius:14px;padding:4px 12px;"
            "font-size:13px;font-weight:600;"
        );

        bottomLay->addWidget(input, 1);
        bottomLay->addWidget(send);

        root->addWidget(headerName);
        root->addLayout(convLayout, 1);
        root->addWidget(bottom);
    }

    void setContact(const QString &name) {
        headerName->setText("Conversation with " + name);
    }
};

// ChatRow: one line in the chat list, clickable
class ChatRow : public QWidget {
public:
    QString contactName;
    std::function<void(const QString&)> onClick;

    ChatRow(const QString &name,
            const QString &preview,
            const QString &time,
            QWidget *parent=nullptr) : QWidget(parent), contactName(name)
    {
        QHBoxLayout *h = new QHBoxLayout(this);
        h->setContentsMargins(0, 10, 0, 10);
        h->setSpacing(12);

        QLabel *avatar = new QLabel(name.left(1), this);
        avatar->setAlignment(Qt::AlignCenter);
        avatar->setFixedSize(36, 36);
        avatar->setStyleSheet(
            "background:#ffffff;"
            "color:#0f172a;font-weight:600;font-size:16px;"
            "border-radius:18px;"
        );

        QVBoxLayout *v = new QVBoxLayout;
        v->setSpacing(2);

        QLabel *nameLbl = new QLabel(name, this);
        nameLbl->setStyleSheet("font-weight:600;font-size:14px;color:#0b1120;");

        QLabel *previewLbl = new QLabel(preview, this);
        previewLbl->setStyleSheet("font-size:12px;color:#4b5563;");
        previewLbl->setWordWrap(false);

        v->addWidget(nameLbl);
        v->addWidget(previewLbl);

        QLabel *timeLbl = new QLabel(time, this);
        timeLbl->setAlignment(Qt::AlignRight | Qt::AlignTop);
        timeLbl->setStyleSheet("font-size:11px;color:#9ca3af;");

        h->addWidget(avatar);
        h->addLayout(v, 1);
        h->addWidget(timeLbl);

        setMouseTracking(true);
        setCursor(Qt::PointingHandCursor);
        setStyleSheet(
            "ChatRow { background: transparent; }"
            "ChatRow:hover { background: rgba(0,0,0,0.03); }"
        );
    }

protected:
    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton && onClick) {
            onClick(contactName);
        }
        QWidget::mousePressEvent(event);
    }
};

// ChatsPage: shows the contacts list and a search bar, emits via callback
class ChatsPage : public QWidget {
public:
    std::function<void(const QString&)> onChatSelected;

    ChatsPage(QWidget *parent=nullptr) : QWidget(parent) {
        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(40, 32, 40, 32);
        root->setSpacing(16);

        QLabel *title = new QLabel("Chats", this);
        title->setStyleSheet("font-size:20px;font-weight:600;color:#0b1120;");

        QLineEdit *search = new QLineEdit(this);
        search->setPlaceholderText("Search");
        search->setStyleSheet(
            "background:rgba(255,255,255,0.85);"
            "border-radius:18px;border:none;"
            "padding:8px 12px;font-size:13px;"
        );

        QWidget *list = new QWidget(this);
        QVBoxLayout *listLay = new QVBoxLayout(list);
        listLay->setContentsMargins(0, 8, 0, 8);
        listLay->setSpacing(0);

        auto addRow = [&](const QString &name,
                          const QString &preview,
                          const QString &time) {
            QWidget *rowContainer = new QWidget(list);
            QVBoxLayout *rowLayout = new QVBoxLayout(rowContainer);
            rowLayout->setContentsMargins(0,0,0,0);
            rowLayout->setSpacing(0);

            ChatRow *row = new ChatRow(name, preview, time, rowContainer);
            // callback vers l'extérieur
            row->onClick = [this](const QString &contact){
                if(onChatSelected) onChatSelected(contact);
            };

            rowLayout->addWidget(row);

            QFrame *sep = new QFrame(rowContainer);
            sep->setFrameShape(QFrame::HLine);
            sep->setFrameShadow(QFrame::Plain);
            sep->setStyleSheet("color:rgba(15,23,42,0.12);");
            sep->setFixedHeight(1);

            rowLayout->addWidget(sep);
            listLay->addWidget(rowContainer);
        };

        addRow("Jane", "a envoyé un fichier", "09:41");
        addRow("John", "a envoyé un fichier", "09:41");
        listLay->addStretch();

        root->addWidget(title);
        root->addWidget(search);
        root->addWidget(list);
    }
};

// FilesPage: shows secure files as large cards
class FilesPage : public QWidget {
public:
    FilesPage(QWidget *parent=nullptr) : QWidget(parent) {
        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(48,48,48,48);
        root->setSpacing(24);

        QLabel *title = new QLabel("Secure Files", this);
        title->setStyleSheet("font-size:26px;font-weight:700;");

        QLabel *subtitle = new QLabel(
            "Sensitive documents, organized and encrypted.",
            this
        );
        subtitle->setStyleSheet("font-size:13px;color:#374151;");

        QLineEdit *search = new QLineEdit(this);
        search->setPlaceholderText("Search files…");
        search->setStyleSheet(
            "background:rgba(255,255,255,0.95);"
            "border-radius:18px;border:none;padding:10px 14px;"
        );

        QWidget *listContainer = new QWidget(this);
        QVBoxLayout *listLayout = new QVBoxLayout(listContainer);
        listLayout->setContentsMargins(0,0,0,0);
        listLayout->setSpacing(16);

        auto addFileCard = [&](const QString &name,
                               const QString &typeLabel,
                               const QString &size,
                               const QString &date) {

            QWidget *card = new QWidget(this);
            card->setStyleSheet(
                "background:rgba(255,255,255,0.98);"
                "border-radius:18px;"
                "padding:14px 18px;"
            );

            QHBoxLayout *h = new QHBoxLayout(card);
            h->setContentsMargins(0,0,0,0);
            h->setSpacing(12);

            QLabel *icon = new QLabel(typeLabel, card);
            icon->setAlignment(Qt::AlignCenter);
            icon->setFixedSize(40,40);
            icon->setStyleSheet(
                "background:#ef4444;"
                "color:white;font-weight:700;font-size:12px;"
                "border-radius:12px;"
            );

            QVBoxLayout *vText = new QVBoxLayout;
            QLabel *nameLbl = new QLabel(name, card);
            nameLbl->setStyleSheet("font-weight:600;font-size:14px;");

            QLabel *metaLbl = new QLabel(size + "  •  " + date, card);
            metaLbl->setStyleSheet("font-size:12px;color:#6b7280;");

            vText->addWidget(nameLbl);
            vText->addWidget(metaLbl);

            QPushButton *actionBtn = new QPushButton("Share", card);
            actionBtn->setFixedHeight(30);
            actionBtn->setStyleSheet(
                "background:" + kPrimaryBlue() + ";"
                "color:white;font-size:12px;font-weight:600;"
                "border-radius:15px;padding:0 14px;"
            );

            h->addWidget(icon);
            h->addLayout(vText, 1);
            h->addStretch();
            h->addWidget(actionBtn);

            listLayout->addWidget(card);
        };

        addFileCard("File1.pdf",  "PDF",  "2.3 MB", "Today, 09:41");
        addFileCard("File2.pdf",  "PDF",  "780 KB", "Yesterday, 18:12");
        addFileCard("Image1.png", "PNG",  "1.1 MB", "Dec 01, 14:32");
        addFileCard("File3.pdf",  "PDF",  "4.8 MB", "Nov 28, 10:05");

        listLayout->addStretch();

        root->addWidget(title);
        root->addWidget(subtitle);
        root->addSpacing(8);
        root->addWidget(search);
        root->addSpacing(16);
        root->addWidget(listContainer);
    }
};

// AlertPage: shows alert actions in a card
class AlertPage : public QWidget {
public:
    AlertPage(QWidget *parent=nullptr) : QWidget(parent) {
        QVBoxLayout *root = new QVBoxLayout(this);
        root->setContentsMargins(48, 48, 48, 48);
        root->setSpacing(24);

        QLabel *title = new QLabel("Alert Center", this);
        title->setStyleSheet("font-size:26px;font-weight:700;color:#0b1120;");

        QLabel *subtitle = new QLabel(
            "Use these actions to notify security or broadcast an alert.",
            this
        );
        subtitle->setStyleSheet("font-size:13px;color:#374151;");
        subtitle->setWordWrap(true);

        QWidget *alertCard = new QWidget(this);
        alertCard->setStyleSheet(
            "background:rgba(255,255,255,0.98);"
            "border-radius:24px;"
            "padding:24px 24px;"
        );
        QVBoxLayout *alertLayout = new QVBoxLayout(alertCard);
        alertLayout->setContentsMargins(0,0,0,0);
        alertLayout->setSpacing(18);

        QWidget *banner = new QWidget(alertCard);
        banner->setStyleSheet(
            "background:" + kRedAlert() + ";"
            "border-radius:18px;"
        );
        QHBoxLayout *bannerLay = new QHBoxLayout(banner);
        bannerLay->setContentsMargins(16,12,16,12);
        bannerLay->setSpacing(12);

        QLabel *icon = new QLabel("!", banner);
        icon->setAlignment(Qt::AlignCenter);
        icon->setFixedSize(32,32);
        icon->setStyleSheet(
            "background:white;color:" + kRedAlert() + ";"
            "font-weight:900;font-size:18px;border-radius:16px;"
        );

        QLabel *bannerText = new QLabel("SECURITY ALERT", banner);
        bannerText->setStyleSheet(
            "color:white;font-weight:700;font-size:16px;"
        );

        bannerLay->addWidget(icon);
        bannerLay->addWidget(bannerText);
        bannerLay->addStretch();

        auto makeActionButton = [](const QString &text, const QString &color) {
            QPushButton *b = new QPushButton(text);
            b->setMinimumHeight(40);
            b->setStyleSheet(
                "background:" + color + ";"
                "color:white;font-weight:600;font-size:13px;"
                "border:none;border-radius:20px;"
            );
            return b;
        };

        QPushButton *btnCallSupport = makeActionButton(
            "CALL SECURITY SUPPORT", kRedAlert());
        QPushButton *btnGeneralAlert = makeActionButton(
            "SEND GENERAL ALERT", "#f97316");
        QPushButton *btnSendLocation = makeActionButton(
            "SEND LIVE LOCATION", kPrimaryBlue());

        QPushButton *btnCancel = new QPushButton("CANCEL", alertCard);
        btnCancel->setMinimumHeight(36);
        btnCancel->setStyleSheet(
            "background:transparent;"
            "border:1px solid rgba(15,23,42,0.15);"
            "border-radius:18px;"
            "font-weight:500;font-size:13px;color:#111827;"
        );

        alertLayout->addWidget(banner);
        alertLayout->addSpacing(4);
        alertLayout->addWidget(btnCallSupport);
        alertLayout->addWidget(btnGeneralAlert);
        alertLayout->addWidget(btnSendLocation);
        alertLayout->addSpacing(4);
        alertLayout->addWidget(btnCancel);

        root->addWidget(title);
        root->addWidget(subtitle);
        root->addSpacing(12);
        root->addWidget(alertCard, 0, Qt::AlignTop);
        root->addStretch();
    }
};

// ---------- MAIN WINDOW (style Mac / desktop) ----------

class MainWindow : public QMainWindow {
public:
    QStackedWidget *stack;
    ConversationPage *convPage;

    MainWindow(QWidget *parent = nullptr);
};

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1400, 850);
    setWindowTitle("SecureCloud – Desktop");

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QHBoxLayout *root = new QHBoxLayout(central);
    root->setContentsMargins(0,0,0,0);
    root->setSpacing(0);

    QWidget *sidebar = new QWidget(this);
    sidebar->setFixedWidth(260);
    sidebar->setStyleSheet(
        "background:#020617;"
        "color:white;"
        "border-right:1px solid rgba(15,23,42,0.6);"
    );

    QVBoxLayout *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(24,24,24,24);
    sideLayout->setSpacing(16);

    QLabel *appTitle = new QLabel("SECURE CLOUD", sidebar);
    appTitle->setStyleSheet(
        "font-size:20px;font-weight:700;letter-spacing:2px;"
        "color:" + kRedAlert() + ";"
    );

    QLabel *statusChip = new QLabel("Welcome", sidebar);
    statusChip->setAlignment(Qt::AlignCenter);
    statusChip->setFixedHeight(24);
    statusChip->setStyleSheet(
        "background:rgba(15,23,42,0.8);"
        "border-radius:12px;"
        "font-size:11px;color:#9ca3af;"
    );

    QPushButton *btnMap   = new QPushButton("Map", sidebar);
    QPushButton *btnChats = new QPushButton("Chats", sidebar);
    QPushButton *btnFiles = new QPushButton("Secure Files", sidebar);
    QPushButton *btnAlert = new QPushButton("Alert Center", sidebar);

    QPushButton *btnWarning = new QPushButton("Warning", sidebar);
    btnWarning->setFixedHeight(42);
    btnWarning->setStyleSheet(
        "background:" + kRedAlert() + ";"
        "color:white;"
        "font-weight:700;"
        "font-size:15px;"
        "border-radius:21px;"
        "padding:0 14px;"
    );

    QString sideBtnStyle =
        "QPushButton{"
        "  background:transparent;"
        "  border:none;"
        "  color:#e5e7eb;"
        "  text-align:left;"
        "  padding:10px 8px;"
        "  font-size:14px;"
        "}"
        "QPushButton:hover{"
        "  background:#111827;"
        "}"
        "QPushButton:pressed{"
        "  background:#1f2937;"
        "}";

    btnMap->setStyleSheet(sideBtnStyle);
    btnChats->setStyleSheet(sideBtnStyle);
    btnFiles->setStyleSheet(sideBtnStyle);
    btnAlert->setStyleSheet(sideBtnStyle);

    sideLayout->addWidget(appTitle);
    sideLayout->addWidget(statusChip);
    sideLayout->addSpacing(16);
    sideLayout->addWidget(btnMap);
    sideLayout->addWidget(btnChats);
    sideLayout->addWidget(btnFiles);
    sideLayout->addWidget(btnAlert);
    sideLayout->addStretch();
    sideLayout->addWidget(btnWarning, 0, Qt::AlignLeft);

    QWidget *right = new QWidget(this);
    right->setStyleSheet("background:" + kBlueBg() + ";");
    QVBoxLayout *rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(0,0,0,0);
    rightLayout->setSpacing(0);

    QWidget *topBar = new QWidget(right);
    topBar->setFixedHeight(48);
    topBar->setStyleSheet(
        "background:rgba(255,255,255,0.7);"
        "border-bottom:1px solid rgba(148,163,184,0.6);"
    );
    QHBoxLayout *topLayout = new QHBoxLayout(topBar);
    topLayout->setContentsMargins(16,4,16,4);

    QLabel *sectionTitle = new QLabel("", topBar);
    sectionTitle->setStyleSheet("font-size:15px;font-weight:600;color:#0b1120;");

    QLabel *userLabel = new QLabel("Signed in as Alice", topBar);
    userLabel->setStyleSheet("font-size:12px;color:#6b7280;");

    topLayout->addWidget(sectionTitle);
    topLayout->addStretch();
    topLayout->addWidget(userLabel);

    stack = new QStackedWidget(right);
    MapPage       *mapPage    = new MapPage(stack);
    ChatsPage     *chatsPage  = new ChatsPage(stack);
    FilesPage     *filesPage  = new FilesPage(stack);
    AlertPage     *alertPage  = new AlertPage(stack);
    convPage                  = new ConversationPage(stack);

    int mapIdx   = stack->addWidget(mapPage);
    int chatsIdx = stack->addWidget(chatsPage);
    int filesIdx = stack->addWidget(filesPage);
    int alertIdx = stack->addWidget(alertPage);
    int convIdx  = stack->addWidget(convPage);

    stack->setCurrentIndex(mapIdx);

    rightLayout->addWidget(topBar);
    rightLayout->addWidget(stack);

    root->addWidget(sidebar);
    root->addWidget(right, 1);

    QObject::connect(btnMap,   &QPushButton::clicked, this, [=]() { stack->setCurrentIndex(mapIdx);   });
    QObject::connect(btnChats, &QPushButton::clicked, this, [=]() { stack->setCurrentIndex(chatsIdx); });
    QObject::connect(btnFiles, &QPushButton::clicked, this, [=]() { stack->setCurrentIndex(filesIdx); });
    QObject::connect(btnAlert, &QPushButton::clicked, this, [=]() { stack->setCurrentIndex(alertIdx); });
    QObject::connect(btnWarning, &QPushButton::clicked, this, [](){
        qDebug() << "Warning button clicked";
    });

    // Quand on clique sur un chat, on ouvre ConversationPage avec le nom
    chatsPage->onChatSelected = [this, convIdx](const QString &name){
        convPage->setContact(name);
        this->stack->setCurrentIndex(convIdx);
    };
}

// ---------- LOGIN WINDOW (shown first) ----------

class LoginWindow : public QMainWindow {
public:
    QLineEdit *emailEdit;
    QLineEdit *passwordEdit;

    LoginWindow(QWidget *parent=nullptr) : QMainWindow(parent) {
        resize(500, 600);
        setWindowTitle("SecureCloud – Login");

        QWidget *central = new QWidget(this);
        setCentralWidget(central);

        QVBoxLayout *root = new QVBoxLayout(central);
        root->setContentsMargins(60, 60, 60, 60);

        QLabel *logo = new QLabel("SECURE CLOUD", this);
        logo->setAlignment(Qt::AlignCenter);
        logo->setStyleSheet(
            "font-size:28px;font-weight:700;"
            "letter-spacing:2px;color:" + kRedAlert() + ";"
        );

        QWidget *card = new QWidget(this);
        card->setStyleSheet(
            "background:rgba(255,255,255,0.96);"
            "border-radius:24px;"
            "border:1px solid rgba(255,255,255,0.8);"
        );
        QVBoxLayout *cardLay = new QVBoxLayout(card);
        cardLay->setContentsMargins(32, 32, 32, 32);
        cardLay->setSpacing(16);

        emailEdit = new QLineEdit(card);
        emailEdit->setPlaceholderText("Email");
        emailEdit->setStyleSheet(
            "background:#f2f4ff;border-radius:16px;border:none;"
            "padding:10px 14px;"
        );

        passwordEdit = new QLineEdit(card);
        passwordEdit->setPlaceholderText("Password");
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setStyleSheet(emailEdit->styleSheet());

        QPushButton *forgot = new QPushButton("Forget Password ?", card);
        forgot->setFlat(true);
        forgot->setStyleSheet(
            "color:" + kPrimaryBlue() + ";font-size:13px;text-align:left;"
        );

        QPushButton *loginBtn = new QPushButton("LOGIN", card);
        loginBtn->setStyleSheet(
            "background:" + kPrimaryBlue() + ";color:white;"
            "font-weight:600;border-radius:18px;padding:10px 0;"
        );

        QPushButton *helpBtn = new QPushButton("HELP", card);
        helpBtn->setStyleSheet(
            "background:rgba(255,255,255,0.7);"
            "border-radius:18px;padding:8px 0;"
        );

        cardLay->addWidget(emailEdit);
        cardLay->addWidget(passwordEdit);
        cardLay->addWidget(forgot);
        cardLay->addSpacing(4);
        cardLay->addWidget(loginBtn);
        cardLay->addWidget(helpBtn);

        root->addWidget(logo);
        root->addSpacing(24);
        root->addWidget(card, 0, Qt::AlignCenter);
        root->addStretch();

        QObject::connect(loginBtn, &QPushButton::clicked, this, [this]() {
            if (emailEdit->text() == "test@secure.com"
                && passwordEdit->text() == "1234") {
                MainWindow *mw = new MainWindow();
                mw->show();
                this->close();
            } else {
                QMessageBox::warning(this, "Error",
                                     "Invalid email or password.");
            }
        });
    }
};

// ---------- main ----------

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setStyleSheet(
        "QMainWindow, QWidget {"
        "  font-family:-apple-system,'SF Pro Text','Segoe UI',sans-serif;"
        "  font-size:13px;"
        "  color:#0b1120;"
        "}"
        "QLineEdit {"
        "  selection-background-color:#2563eb;"
        "}"
        "QPushButton {"
        "  font-family:-apple-system,'SF Pro Text','Segoe UI',sans-serif;"
        "}"
    );

    LoginWindow w;
    w.show();

    return app.exec();
}
push