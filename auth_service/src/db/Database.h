#pragma once
#include <pqxx/pqxx>
#include <optional>
#include <string>
#include <vector>
#include <mutex>

struct UserRecord {
    int id;                 // maps to users.id_users
    std::string email;      // users.email
    std::string password_hash; // users.password_hash
    std::string full_name;  // users.full_name
    int role_id;           // users.role_id (nullable)
    std::string role_name; // roles.name
};

struct RoleRecord {
    int id;
    std::string name;
    std::string description;
    std::vector<std::string> permissions;
};

class Database {
private:
    std::string connStr_;            // original connection string (for reconnect)
    pqxx::connection conn;           // active connection

    // pqxx::connection / transactions are not thread-safe.
    // gRPC handlers can run concurrently, so serialize DB access.
    mutable std::recursive_mutex mutex_;

    // Assure qu'une connexion ouverte est disponible, sinon tente une reconnexion.
    void ensureConnection();

public:
    explicit Database(const std::string& connStr);
    bool userExists(const std::string& email);
    void registerUser(const std::string& fullName, const std::string& email, const std::string& hashedPassword);
    void registerUserWithRole(const std::string& fullName, const std::string& email, const std::string& hashedPassword, const std::string& roleName);
    std::optional<UserRecord> getUserByEmail(const std::string& email);
    std::optional<UserRecord> getUserById(int user_id);

    // Auth helpers
    bool updateUserPasswordHash(int user_id, const std::string& new_hashed_password);
    std::optional<std::string> getUserTokenJti(int user_id);
    bool setUserTokenJti(int user_id, const std::string& token_jti);
    bool isTokenRevoked(const std::string& token_jti);
    bool revokeTokenJti(const std::string& token_jti);

    std::vector<UserRecord> listUsers();

    // Admin helpers
    bool deleteUserById(int user_id);
    
    // RBAC methods
    std::vector<std::string> getUserPermissions(int user_id);
    bool isUserAdmin(const std::string& email);
    bool assignRoleToUser(int user_id, int role_id);
    RoleRecord getRole(int role_id);
    std::optional<int> getRoleIdByName(const std::string& role_name);
    bool createRole(const std::string& name, const std::string& description);
    std::vector<RoleRecord> listRoles();
    bool addPermissionToRole(int role_id, const std::string& permission);
};
