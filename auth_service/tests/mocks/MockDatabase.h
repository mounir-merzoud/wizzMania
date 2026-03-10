#pragma once

#include <gmock/gmock.h>
#include "db/Database.h"

/**
 * @brief Mock de la classe Database pour les tests unitaires de l'auth_service.
 *
 * Toutes les méthodes publiques de Database sont mockées afin de tester
 * AuthServiceImpl sans dépendance à PostgreSQL.
 */
class MockDatabase : public Database {
public:
    // Constructeur : on passe une chaîne vide pour ne pas tenter de connexion réelle.
    MockDatabase() : Database("") {}

    MOCK_METHOD(bool, userExists,
                (const std::string& email), (override));

    MOCK_METHOD(void, registerUser,
                (const std::string& fullName,
                 const std::string& email,
                 const std::string& hashedPassword), (override));

    MOCK_METHOD(void, registerUserWithRole,
                (const std::string& fullName,
                 const std::string& email,
                 const std::string& hashedPassword,
                 const std::string& roleName), (override));

    MOCK_METHOD(std::optional<UserRecord>, getUserByEmail,
                (const std::string& email), (override));

    MOCK_METHOD(std::vector<UserRecord>, listUsers, (), (override));

    MOCK_METHOD(std::vector<std::string>, getUserPermissions,
                (int user_id), (override));

    MOCK_METHOD(bool, isUserAdmin,
                (const std::string& email), (override));

    MOCK_METHOD(bool, assignRoleToUser,
                (int user_id, int role_id), (override));

    MOCK_METHOD(RoleRecord, getRole,
                (int role_id), (override));

    MOCK_METHOD(bool, createRole,
                (const std::string& name,
                 const std::string& description), (override));

    MOCK_METHOD(std::vector<RoleRecord>, listRoles, (), (override));

    MOCK_METHOD(bool, addPermissionToRole,
                (int role_id,
                 const std::string& permission), (override));
};
