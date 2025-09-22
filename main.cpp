#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <vector>
#include <cstdlib>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>

class WizzManiaLauncher {
public:
    static void showBanner() {
        std::cout << "\n";
        std::cout << "🏥 ╔══════════════════════════════════════╗ 🏥\n";
        std::cout << "   ║        WizzMania Medical App         ║\n";
        std::cout << "   ║     Secure Communication Platform   ║\n";
        std::cout << "   ║        for Medical Professionals    ║\n";
        std::cout << "🔐 ╚══════════════════════════════════════╝ 🔐\n";
        std::cout << "\n";
    }

    static void showHelp() {
        showBanner();
        std::cout << "📋 Commandes disponibles:\n\n";
        std::cout << "  🏥 ./main server     - Démarrer le serveur médical\n";
        std::cout << "  👤 ./main client     - Démarrer le client médical\n";
        std::cout << "  🎬 ./main demo       - Démo automatique (serveur + client)\n";
        std::cout << "  ⚙️  ./main setup      - Configuration initiale\n";
        std::cout << "  🧹 ./main clean      - Nettoyer les fichiers temporaires\n";
        std::cout << "  ❓ ./main --help     - Afficher cette aide\n\n";
        
        std::cout << "🔧 Options avancées:\n";
        std::cout << "  ./main server --http-only   - Serveur HTTP uniquement\n";
        std::cout << "  ./main server --https-only  - Serveur HTTPS uniquement\n";
        std::cout << "  ./main client --simple      - Client simple (HTTP)\n\n";
        
        std::cout << "📖 Exemples d'utilisation:\n";
        std::cout << "  make setup && ./main demo   - Installation + démo\n";
        std::cout << "  ./main server &             - Serveur en arrière-plan\n";
        std::cout << "  ./main client               - Client interactif\n\n";
    }

    static int runServer(const std::vector<std::string>& args) {
        std::cout << "🏥 Démarrage du serveur médical...\n";
        
        // Check if server binary exists
        if (access("server/medical_server", F_OK) != 0) {
            std::cout << "❌ Serveur non compilé. Compilation en cours...\n";
            system("cd server && make");
        }
        
        // Check for SSL certificates
        if (access("server/server.crt", F_OK) != 0 || access("server/server.key", F_OK) != 0) {
            std::cout << "🔐 Génération des certificats SSL...\n";
            system("cd server && make certs");
        }
        
        std::cout << "✅ Serveur prêt!\n";
        std::cout << "🌐 HTTP  : http://localhost:8080\n";
        std::cout << "🔒 HTTPS : https://localhost:8443\n";
        std::cout << "📊 Health: http://localhost:8080/health\n\n";
        
        // Handle special args
        std::string server_cmd = "cd server && ./medical_server";
        
        return system(server_cmd.c_str());
    }

    static int runClient(const std::vector<std::string>& args) {
        std::cout << "👤 Démarrage du client médical...\n";
        
        // Check for simple mode
        bool simple_mode = false;
        for (const auto& arg : args) {
            if (arg == "--simple") {
                simple_mode = true;
                break;
            }
        }
        
        // Check if client binary exists
        std::string client_binary = simple_mode ? "client_server/simple_http_client" : "client_server/medical_client";
        if (access(client_binary.c_str(), F_OK) != 0) {
            std::cout << "❌ Client non compilé. Compilation en cours...\n";
            system("cd client_server && make");
        }
        
        // Check if server is running
        std::cout << "🔍 Vérification du serveur...\n";
        int server_check = system("curl -s http://localhost:8080/health > /dev/null 2>&1");
        
        if (server_check != 0) {
            std::cout << "⚠️  Serveur non détecté. Voulez-vous le démarrer? (y/n): ";
            char response;
            std::cin >> response;
            std::cin.ignore(); // Pour nettoyer le buffer
            if (response == 'y' || response == 'Y') {
                std::cout << "🏥 Démarrage du serveur en arrière-plan...\n";
                system("./main server &");
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        } else {
            std::cout << "✅ Serveur détecté et fonctionnel!\n";
        }
        
        std::cout << "🚀 Lancement du client";
        if (simple_mode) {
            std::cout << " simple (HTTP seulement)";
        }
        std::cout << "...\n\n";
        
        std::string client_cmd = simple_mode ? 
            "cd client_server && ./simple_http_client" : 
            "cd client_server && ./medical_client";
        
        return system(client_cmd.c_str());
    }

    static int runDemo() {
        showBanner();
        std::cout << "🎬 === DÉMO WIZZMANIA ===\n\n";
        
        std::cout << "🔧 Étape 1: Vérification de l'environnement...\n";
        
        // Check dependencies
        if (system("which curl > /dev/null 2>&1") != 0) {
            std::cout << "❌ curl non trouvé. Installation requise.\n";
            return 1;
        }
        
        // Build everything
        std::cout << "🔨 Étape 2: Compilation...\n";
        system("make all");
        
        // Generate certificates
        std::cout << "🔐 Étape 3: Certificats SSL...\n";
        system("make certs");
        
        std::cout << "🏥 Étape 4: Démarrage du serveur...\n";
        
        // Start server in background
        pid_t server_pid = fork();
        if (server_pid == 0) {
            // Child process - run server
            system("cd server && ./medical_server");
            exit(0);
        } else if (server_pid > 0) {
            // Parent process
            std::cout << "⏳ Attente du démarrage du serveur...\n";
            std::this_thread::sleep_for(std::chrono::seconds(3));
            
            // Test server
            std::cout << "🧪 Test de connectivité...\n";
            int test_result = system("curl -s http://localhost:8080/health");
            
            if (test_result == 0) {
                std::cout << "✅ Serveur opérationnel!\n\n";
                std::cout << "👤 Vous pouvez maintenant lancer le client avec:\n";
                std::cout << "   ./main client\n\n";
                std::cout << "🌐 Ou tester via curl:\n";
                std::cout << "   curl http://localhost:8080/health\n";
                std::cout << "   curl -X POST http://localhost:8080/api/v1/auth/login \\\n";
                std::cout << "        -H 'Content-Type: application/json' \\\n";
                std::cout << "        -d '{\"username\":\"docteur\",\"password\":\"test\"}'\n\n";
                
                std::cout << "⏹️  Pour arrêter le serveur: kill " << server_pid << "\n";
            } else {
                std::cout << "❌ Erreur de démarrage du serveur\n";
                kill(server_pid, SIGTERM);
                return 1;
            }
        } else {
            std::cout << "❌ Erreur de création du processus serveur\n";
            return 1;
        }
        
        return 0;
    }

    static int runSetup() {
        showBanner();
        std::cout << "⚙️ === CONFIGURATION WIZZMANIA ===\n\n";
        
        std::cout << "📦 Installation des dépendances...\n";
        system("make deps");
        
        std::cout << "🔐 Génération des certificats SSL...\n";
        system("make certs");
        
        std::cout << "🔨 Compilation de tous les composants...\n";
        system("make all");
        
        std::cout << "\n✅ Configuration terminée!\n";
        std::cout << "🚀 Vous pouvez maintenant utiliser:\n";
        std::cout << "   ./main demo    - Pour une démonstration\n";
        std::cout << "   ./main server  - Pour démarrer le serveur\n";
        std::cout << "   ./main client  - Pour démarrer le client\n\n";
        
        return 0;
    }

    static int runClean() {
        std::cout << "🧹 Nettoyage des fichiers temporaires...\n";
        
        system("make clean");
        system("rm -f *.log");
        system("rm -f core.*");
        
        std::cout << "✅ Nettoyage terminé!\n";
        return 0;
    }
};

int main(int argc, char* argv[]) {
    std::vector<std::string> args;
    for (int i = 1; i < argc; i++) {
        args.push_back(std::string(argv[i]));
    }
    
    if (args.empty() || args[0] == "--help" || args[0] == "-h") {
        WizzManiaLauncher::showHelp();
        return 0;
    }
    
    std::string command = args[0];
    
    if (command == "server") {
        return WizzManiaLauncher::runServer(args);
    } else if (command == "client") {
        return WizzManiaLauncher::runClient(args);
    } else if (command == "demo") {
        return WizzManiaLauncher::runDemo();
    } else if (command == "setup") {
        return WizzManiaLauncher::runSetup();
    } else if (command == "clean") {
        return WizzManiaLauncher::runClean();
    } else {
        std::cout << "❌ Commande inconnue: " << command << "\n";
        std::cout << "Utilisez './main --help' pour voir les options disponibles.\n";
        return 1;
    }
    
    return 0;
}