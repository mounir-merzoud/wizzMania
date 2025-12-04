# Configuration Postman pour tester le service gRPC d'authentification

## 📋 Informations de connexion
- **Serveur**: `localhost:50061`
- **Protocole**: gRPC (insecure/non-TLS)
- **Fichier Proto**: `c:\Users\lo\Desktop\bbt\wizzMania\auth_service\proto\auth.proto`

## 🚀 Étapes de configuration dans Postman

### 1. Créer une nouvelle requête gRPC
1. Ouvrir Postman
2. Cliquer sur "New" → "gRPC Request"
3. Entrer l'URL du serveur: `localhost:50061`
4. Sélectionner "Server reflection" OU importer le fichier proto

### 2. Importer le fichier proto (méthode recommandée)
1. Aller dans l'onglet "Proto" 
2. Cliquer sur "Select Proto Files"
3. Naviguer vers: `c:\Users\lo\Desktop\bbt\wizzMania\auth_service\proto\auth.proto`
4. Sélectionner le fichier et l'importer

### 3. Configuration des méthodes disponibles
Après import, vous devriez voir ces méthodes dans `auth.AuthService`:
- `Login`
- `RegisterUser` 
- `ValidateToken`
- `RefreshToken`
- `AssignRole`
- `CreateRole`
- `ListRoles`
- `GetUserPermissions`

## 🔐 Test 1: Connexion Admin

### Méthode: `auth.AuthService/Login`
### Payload JSON:
```json
{
  "email": "admin@wizzmania.com",
  "password": "AdminPass123!"
}
```

### Résultat attendu:
```json
{
  "access_token": "eyJ0eXAiOiJKV1QiLCJhbGciOiJIUzI1NiJ9...",
  "refresh_token": "",
  "expires_in": "86400",
  "role": "admin",
  "permissions": [
    "user.create",
    "user.read", 
    "user.update",
    "user.delete",
    "role.create",
    "role.assign"
  ]
}
```

## 🧪 Test 2: Création d'utilisateur (Admin uniquement)

### Méthode: `auth.AuthService/RegisterUser`
### Payload JSON:
```json
{
  "admin_token": "[TOKEN_ADMIN_DU_LOGIN]",
  "email": "user1@wizzmania.com",
  "password": "UserPass123!",
  "full_name": "Utilisateur Test"
}
```

## 🔍 Test 3: Validation de token

### Méthode: `auth.AuthService/ValidateToken`  
### Payload JSON:
```json
{
  "token": "[TOKEN_A_VALIDER]"
}
```

## 📝 Notes importantes
- Le service fonctionne sur le port **50061** (pas 50051)
- L'admin existe déjà avec email: `admin@wizzmania.com` et mot de passe: `AdminPass123!`
- Tous les tokens ont une durée de vie de 24h
- Les rôles et permissions doivent apparaître dans la réponse Login si RBAC fonctionne correctement