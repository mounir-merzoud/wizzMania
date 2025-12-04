-- Script pour corriger l'utilisateur user1@wizzmania.com sans rôle

-- 1. Vérifier l'état actuel
SELECT 
    u.id_users, 
    u.email, 
    u.role_id, 
    r.name as role_name 
FROM users u 
LEFT JOIN roles r ON u.role_id = r.id_roles 
WHERE u.email = 'user1@wizzmania.com';

-- 2. Obtenir l'ID du rôle "user"
SELECT id_roles FROM roles WHERE name = 'user';

-- 3. Assigner le rôle "user" à user1@wizzmania.com
UPDATE users 
SET role_id = (SELECT id_roles FROM roles WHERE name = 'user')
WHERE email = 'user1@wizzmania.com';

-- 4. Vérification après correction
SELECT 
    u.id_users, 
    u.email, 
    u.role_id, 
    r.name as role_name 
FROM users u 
LEFT JOIN roles r ON u.role_id = r.id_roles 
WHERE u.email = 'user1@wizzmania.com';