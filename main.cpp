#include <QApplication>
#include "src/ui/windows/LoginWindow.h"
#include "src/ui/windows/MainWindow.h"
#include "src/services/AuthService.h"

/**
 * @brief Point d'entrée principal de l'application WizzMania
 * 
 * Cette application centralise toutes les fonctionnalités :
 * - Authentification via auth_service
 * - Messagerie via messaging_service  
 * - Gestion de fichiers via file_service
 * - Interface utilisateur Qt moderne et modulaire
 */
int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    
    // Application metadata
    app.setApplicationName("WizzMania");
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("La Plateforme");
    
    // Global application stylesheet
    app.setStyleSheet(
        "QMainWindow, QWidget {"
        "  font-family:-apple-system,'SF Pro Text','Segoe UI',sans-serif;"
        "  font-size:13px;"
        "}"
    );
    
    // Create login window
    LoginWindow* loginWindow = new LoginWindow();
    MainWindow* mainWindow = nullptr;
    
    // Connect login success signal
    QObject::connect(loginWindow, &LoginWindow::loginSuccessful, 
                     [&](const QString& username) {
        // Hide login window
        loginWindow->hide();
        
        // Create and show main window
        mainWindow = new MainWindow();
        mainWindow->show();

        // If the backend invalidates the session (e.g. refresh revoked), return to login.
        QObject::connect(&AuthService::instance(), &AuthService::userLoggedOut, [&]() {
            if (mainWindow) {
                mainWindow->close();
                delete mainWindow;
                mainWindow = nullptr;
            }
            loginWindow->show();
        });
        
        // Connect logout signal
        QObject::connect(mainWindow, &MainWindow::logoutRequested, [&]() {
            // Best-effort server logout (revocation) then clear local session.
            AuthService::instance().logoutFromServer();
            AuthService::instance().logout();
        });
    });
    
    // Show login window
    loginWindow->show();
    
    int result = app.exec();
    
    // Cleanup
    delete loginWindow;
    if (mainWindow) {
        delete mainWindow;
    }
    
    return result;
}
