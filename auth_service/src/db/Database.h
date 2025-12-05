#pragma once
#include <pqxx/pqxx>
#include <optional>
#include <string>

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

    // Assure qu'une connexion ouverte est disponible, sinon tente une reconnexion.
    void ensureConnection();

public:
    explicit Database(const std::string& connStr);
    bool userExists(const std::string& email);
    void registerUser(const std::string& fullName, const std::string& email, const std::string& hashedPassword);
    void registerUserWithRole(const std::string& fullName, const std::string& email, const std::string& hashedPassword, const std::string& roleName);
    std::optional<UserRecord> getUserByEmail(const std::string& email);
    
    // RBAC methods
    std::vector<std::string> getUserPermissions(int user_id);
    bool isUserAdmin(const std::string& email);
    bool assignRoleToUser(int user_id, int role_id);
    RoleRecord getRole(int role_id);
    bool createRole(const std::string& name, const std::string& description);
    std::vector<RoleRecord> listRoles();
    bool addPermissionToRole(int role_id, const std::string& permission);
};
