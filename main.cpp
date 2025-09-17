#include <QApplication>
#include <QLabel>

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);

    QLabel label("Hello SecureCloud 🚀 from VS Code!");
    label.resize(400, 80);
    label.show();

    return app.exec();
}
