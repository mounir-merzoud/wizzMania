# Serveur-Client C++

Ce projet implémente un système de communication simple entre un serveur et un client en C++ utilisant les sockets TCP.

## Fonctionnalités

- **Serveur**: Écoute les connexions entrantes sur le port 8080
- **Client**: Se connecte au serveur et permet l'envoi de messages
- Communication bidirectionnelle en temps réel
- Gestion de la déconnexion propre avec la commande "quit"

## Compilation

Pour compiler les programmes, utilisez le Makefile fourni :

```bash
# Compiler le serveur et le client
make

# Ou compiler individuellement
make server
make client
```

## Utilisation

### 1. Démarrer le serveur

```bash
# Option 1: Utiliser le Makefile
make run-server

# Option 2: Lancer directement
./server
```

Le serveur affichera :
```
Serveur en écoute sur le port 8080...
```

### 2. Connecter le client

Dans un autre terminal :

```bash
# Option 1: Utiliser le Makefile
make run-client

# Option 2: Lancer directement
./client
```

Le client affichera :
```
Connecté au serveur!
Entrez vos messages (tapez 'quit' pour quitter):
>
```

### 3. Communication

- Tapez vos messages dans le terminal du client et appuyez sur Entrée
- Le serveur recevra le message et enverra une confirmation
- Pour quitter, tapez `quit` dans le client

## Exemple de session

**Terminal du serveur :**
```
Serveur en écoute sur le port 8080...
Client connecté depuis 127.0.0.1
Message reçu du client: Hello Server!
Réponse envoyée au client
Message reçu du client: quit
Client a demandé la déconnexion
```

**Terminal du client :**
```
Connecté au serveur!
Entrez vos messages (tapez 'quit' pour quitter):
> Hello Server!
Message envoyé: Hello Server!
Réponse du serveur: Serveur a reçu: Hello Server!
> quit
Message envoyé: quit
Client fermé.
```

## Architecture

### Serveur (`server.cpp`)
- Utilise la classe `Server` pour encapsuler la logique
- Crée un socket TCP et l'attache au port 8080
- Accepte les connexions entrantes
- Gère la communication avec chaque client
- Renvoie un écho des messages reçus

### Client (`client.cpp`)
- Utilise la classe `Client` pour encapsuler la logique
- Se connecte au serveur sur localhost:8080
- Interface utilisateur pour saisir des messages
- Affiche les réponses du serveur

## Nettoyage

Pour supprimer les fichiers compilés :

```bash
make clean
```

## Configuration

- **Port**: 8080 (défini dans les constantes `PORT`)
- **Adresse**: localhost (127.0.0.1)
- **Taille du buffer**: 1024 octets

## Prérequis

- Compilateur C++ compatible C++11 ou plus récent
- Système Unix/Linux/macOS (utilise les sockets POSIX)
- Make (pour utiliser le Makefile)

## Améliorations possibles

- Support de plusieurs clients simultanés (threading)
- Chiffrement des communications
- Interface graphique
- Configuration via fichier de config
- Logs plus détaillés
