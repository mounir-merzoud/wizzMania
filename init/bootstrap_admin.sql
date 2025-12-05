-- ============================================
-- BOOTSTRAP ADMIN INITIAL - wizzMania
-- ============================================

-- Ce script crée le premier administrateur du système
-- À exécuter APRÈS avoir appliqué schema.sql

-- 🔹 Créer les rôles de base s'ils n'existent pas
INSERT INTO roles (name, description) 
SELECT 'admin', 'Administrateur avec accès complet'
WHERE NOT EXISTS (SELECT 1 FROM roles WHERE name = 'admin');

INSERT INTO roles (name, description) 
SELECT 'user', 'Utilisateur standard'
WHERE NOT EXISTS (SELECT 1 FROM roles WHERE name = 'user');

-- 🔹 Créer des permissions de base
INSERT INTO permissions (name, description) 
SELECT 'user.create', 'Créer des utilisateurs'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'user.create');

INSERT INTO permissions (name, description) 
SELECT 'user.read', 'Lire les informations utilisateur'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'user.read');

INSERT INTO permissions (name, description) 
SELECT 'user.update', 'Modifier les utilisateurs'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'user.update');

INSERT INTO permissions (name, description) 
SELECT 'user.delete', 'Supprimer des utilisateurs'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'user.delete');

INSERT INTO permissions (name, description) 
SELECT 'role.manage', 'Gérer les rôles et permissions'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'role.manage');

INSERT INTO permissions (name, description) 
SELECT 'system.admin', 'Administration système complète'
WHERE NOT EXISTS (SELECT 1 FROM permissions WHERE name = 'system.admin');

-- 🔹 Associer toutes les permissions au rôle admin
INSERT INTO role_permission (id_roles, id_permissions)
SELECT 
    r.id_roles, 
    p.id_permissions
FROM roles r 
CROSS JOIN permissions p 
WHERE r.name = 'admin'
AND NOT EXISTS (
    SELECT 1 FROM role_permission rp 
    WHERE rp.id_roles = r.id_roles AND rp.id_permissions = p.id_permissions
);

-- 🔹 Associer seulement la lecture au rôle user
INSERT INTO role_permission (id_roles, id_permissions)
SELECT 
    r.id_roles, 
    p.id_permissions
FROM roles r 
JOIN permissions p ON p.name = 'user.read'
WHERE r.name = 'user'
AND NOT EXISTS (
    SELECT 1 FROM role_permission rp 
    WHERE rp.id_roles = r.id_roles AND rp.id_permissions = p.id_permissions
);

-- 🔹 CRÉER LE PREMIER ADMIN
-- ⚠️ Mot de passe par défaut: "AdminPass123!"
-- ⚠️ Hash SHA-256 de "AdminPass123!" = A44CA5D29F6DAB4320AB986479FA985B2D584B11A7DA934F7E80BB1449913A07

INSERT INTO users (full_name, email, password_hash, role_id, status) 
SELECT 
    'Administrator', 
    'admin@wizzmania.com', 
    'A44CA5D29F6DAB4320AB986479FA985B2D584B11A7DA934F7E80BB1449913A07', 
    r.id_roles,
    'active'
FROM roles r 
WHERE r.name = 'admin'
AND NOT EXISTS (SELECT 1 FROM users WHERE email = 'admin@wizzmania.com');

-- 🔹 Vérification
SELECT 
    u.full_name, 
    u.email, 
    r.name as role,
    u.created_at
FROM users u 
JOIN roles r ON u.role_id = r.id_roles 
WHERE u.email = 'admin@wizzmania.com';

-- ============================================
-- ✅ BOOTSTRAP TERMINÉ
-- ============================================

-- 📋 INFORMATIONS IMPORTANTES:
-- Email admin: admin@wizzmania.com  
-- Mot de passe: AdminPass123!
-- 
-- ⚠️ CHANGEZ CE MOT DE PASSE EN PRODUCTION !
-- ⚠️ Utilisez ce compte pour créer d'autres admins puis désactivez-le.