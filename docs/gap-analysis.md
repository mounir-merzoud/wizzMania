# SecureCloud / MSF — Gap analysis (état du repo vs cahier des charges)

> Objectif: lister **ce qui reste à faire** en comparant le cahier des charges au code actuel.
> 
> Légende: ✅ fait • 🟡 partiel • ❌ manquant

## 0) Ce qui existe déjà (base technique)

- UI desktop Qt: login + fenêtre principale + pages conversations/contacts (principalement dans `src/ui/windows`).
- Gateway HTTP (JSON) qui proxy vers gRPC auth + messaging: endpoints `/health`, `/login`, `/register`, `/me`, `/users`, `/conversations`, `/conversations/:id/messages`.
  - Implémentation: `gateway_service/src/main.cpp`.
  - Contrat helper JSON: `gateway_service/proto/gateway.proto`.
- Auth gRPC: register/login/validate/list users.
  - Contrat: `auth_service/proto/auth.proto`.
  - Implémentation: `auth_service/src/service/AuthServiceImpl.cpp`.
- Messaging gRPC: SendMessage / GetHistory / stream bidirectionnel (ChatStream).
  - Contrat: `services/messaging_service/proto/messaging.proto`.
- File gRPC: upload/download en chunks + metadata.
  - Contrat: `services/file_service/proto/file_service.proto`.
- DB (PostgreSQL) — schéma complet (roles/permissions/users/messages/files/audit/tokens_revoked…): `init/schema.sql`.
- CI (GitHub Actions) présent mais minimal: `.github/workflows/docker-ci.yml`.

## 1) Auth / sécurité — écarts principaux

### 1.1 Password hashing
- ❌ **Non conforme “secure”**: le hash actuel est du **SHA-256 non salé** (facile à brute-force, pas d’itérations).
  - Code: `auth_service/src/core/PasswordHasher.cpp`.
  - À faire: migrer vers Argon2id / bcrypt / scrypt + salage + paramètres.

### 1.2 Refresh token / session / révocation
- 🟡 Proto prévu (`RefreshToken`, `RevokeTokens`) mais **non implémenté**.
  - Code: `auth_service/src/service/AuthServiceImpl.cpp` retourne `UNIMPLEMENTED`.
- 🟡 DB prévoit `tokens_revoked` + `session` + `users.token_jti` (`init/schema.sql`) mais **non exploité**.
- 🟡 Le login renvoie `refresh_token` vide.
  - Code: `auth_service/src/service/AuthServiceImpl.cpp`.

### 1.3 RBAC (rôles/permissions)
- 🟡 Schéma DB complet (`roles`, `permissions`, `role_permission`, `users.role_id`) ✅
- ❌ RPC RBAC déclarées mais **non implémentées**: `AssignRole`, `CreateRole`, `ListRoles`, `GetUserPermissions`.
  - Code: `auth_service/src/service/AuthServiceImpl.cpp`.

### 1.4 MFA
- ❌ Le proto contient `mfa_code` dans `LoginRequest` mais aucun flux MFA n’est implémenté.
  - Contrat: `auth_service/proto/auth.proto`.
  - À faire: stockage/validation MFA (TOTP/SMS/email selon le cahier), enrollment, recovery.

### 1.5 TLS / transport sécurisé
- ❌ gRPC channels en `InsecureChannelCredentials()` et HTTP en clair.
  - Code: `gateway_service/src/main.cpp`.
  - À faire: TLS côté gateway + mTLS ou TLS interne services selon contraintes.

## 2) Messaging — écarts principaux

### 2.1 “E2E AES-256” réel (client-side)
- 🟡 Le proto transporte `ciphertext` + `nonce` (intention E2E) ✅
  - Contrat: `services/messaging_service/proto/messaging.proto`.
- ❌ **Pas de gestion de clés** (exchange, stockage, rotation, multi-device, etc.) dans le code.
- ❌ UI Qt ne chiffre/déchiffre pas (le front envoie/affiche du `content` en clair via HTTP gateway).
  - Code UI: `src/services/MessagingService.h`.
  - Gateway HTTP: `gateway_service/src/main.cpp` mappe JSON `content` → gRPC.

### 2.2 Temps réel / WebSocket
- 🟡 Le backend messaging a un stream gRPC (`ChatStream`) ✅
- ❌ Le front actuel est en HTTP (polling/refresh). Pas de pont vers stream, et pas d’implémentation WebSocket.
  - UI: `src/services/MessagingService.h`.
  - Gateway: `gateway_service/src/main.cpp`.

### 2.3 Groupes (jusqu’à 100 participants)
- 🟡 Schéma DB supporte `conversation_participant` ✅ (`init/schema.sql`).
- ❌ API/proto/impl ne gèrent pas la création de groupe, la liste des participants, l’ajout/suppression de membres.
  - Proto messaging actuel n’expose que `conversation_id` (pas de membership).

### 2.4 Présence / statuts / notifications
- ❌ Pas de modèle “presence” ni d’événements (online/offline/typing, unread, etc.) côté services.
- 🟡 UI affiche un indicateur de notification local, mais sans mécanisme serveur de push.
  - UI: `src/ui/windows/MainWindow.h`.

### 2.5 Recherche (locale indexée chiffrée)
- ❌ Pas d’index local ni de recherche implémentée.
- ❌ Pas de stratégie de recherche compatible E2E.

## 3) Partage de fichiers — écarts principaux

### 3.1 Intégration UI/gateway
- ❌ Côté UI, le client fichier est un stub.
  - UI: `src/services/FileService.h`.
- ❌ Le gateway HTTP ne proxy pas vers `file_service` (pas d’endpoints exposés).

### 3.2 Persistance / métadonnées
- 🟡 File service stocke sur disque mais la metadata est principalement en mémoire (risque perte au restart).
  - Code: `services/file_service/src/FileServiceImpl.cpp`.
- 🟡 DB a une table `files` + lien `logs_audit` ✅ (`init/schema.sql`) mais **pas d’intégration**.

### 3.3 Compression zlib + déduplication blocs
- ❌ Pas de pipeline compression/dédup implémenté.
- 🟡 `httplib.h` supporte des options de compression HTTP, mais ce n’est pas la même chose qu’une déduplication au niveau stockage.

### 3.4 Contrôle d’accès (RBAC + conversation membership)
- ❌ Pas de checks d’autorisation cohérents “qui peut upload/download quoi”.

## 4) Audit / alerting / compliance

- 🟡 Schéma DB prévoit `logs_audit` et `alerts` ✅ (`init/schema.sql`).
- ❌ Pas de `audit_service` (microservice) ni de pipeline d’écriture/lecture des logs.
- ❌ Pas de génération d’alertes, ni règles, ni endpoints.

## 5) Gateway / API REST

- 🟡 Endpoints REST de base présents (`/login`, `/register`, `/me`, `/users`, conversations/messages) ✅
  - Code: `gateway_service/src/main.cpp`.
- ❌ Manque tout ce qui est prévu côté auth: refresh/revoke, RBAC admin, permissions, etc. (même si le proto auth les déclare).
- ❌ Pas de “real-time” (WebSocket/SSE) exposé.

## 6) Infra / Docker / CI/CD

### 6.1 docker-compose vs CI
- ❌ `docker-compose.yml` **ne démarre pas PostgreSQL**, mais le workflow CI attend `securecloud-db` et appelle `pg_isready`.
  - Compose: `docker-compose.yml`.
  - CI: `.github/workflows/docker-ci.yml`.

### 6.2 Gateway non orchestré
- ❌ Le gateway n’est pas dans `docker-compose.yml` (auth/messaging/file uniquement).

### 6.3 Tests intégration
- 🟡 Des scripts existent dans `test/` mais semblent partiellement désalignés avec ports/champs (à revalider).
- ❌ Le workflow CI ne lance pas de tests applicatifs (il fait surtout `docker compose up` + check DB).

## 7) Checklist “reste à faire” (priorisée)

1) Aligner infra: corriger `docker-compose.yml` et `.github/workflows/docker-ci.yml` (DB/gateway/health checks cohérents).
2) Sécuriser auth: remplacer SHA-256 par un KDF (Argon2id/bcrypt) + migration.
3) Implémenter `RefreshToken` + `RevokeTokens` + stockage session/jti + table `tokens_revoked`.
4) Implémenter RBAC gRPC + exposer via gateway HTTP (admin endpoints) + tests.
5) Décider la stratégie temps réel: WebSocket côté gateway **ou** client gRPC direct côté Qt; implémenter et brancher UI.
6) Implémenter groupes: création, gestion membres, droits, endpoints, UI.
7) Implémenter E2E complet: gestion clés, crypto client (AES-256 + mode authentifié), multi-device, rotation.
8) Fichiers: terminer UI + gateway + persistance DB + autorisations + (si requis) compression/dédup.
9) Audit/alerts: créer le service, écrire logs chiffrés, exploiter `logs_audit`/`alerts`.
10) Mettre des tests CI: smoke tests services, tests d’API, scénarios MSF.
