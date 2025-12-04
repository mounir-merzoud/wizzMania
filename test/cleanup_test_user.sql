-- Nettoyage de l'utilisateur test créé sans rôle
DELETE FROM users WHERE email = 'user1@wizzmania.com';

-- Vérification que les rôles existent bien
SELECT * FROM roles;
SELECT * FROM permissions;