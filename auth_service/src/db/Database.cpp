#include "Database.h"
#include <iostream>

Database::Database(const std::string& connStr)
    : connStr_(connStr), conn(connStr) {}

void Database::ensureConnection() {
    if (!conn.is_open()) {
        std::cerr << "[Database] Connection closed. Attempting reconnect..." << std::endl;
        conn = pqxx::connection(connStr_);
        if (conn.is_open()) {
            std::cerr << "[Database] Reconnection successful." << std::endl;
        } else {
            std::cerr << "[Database] Reconnection failed." << std::endl;
        }
    }
}

bool Database::userExists(const std::string& email) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params("SELECT id_users FROM users WHERE email = $1", email);
        return !r.empty();
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in userExists: " << bc.what() << std::endl;
        // tentative unique de reconnexion
        ensureConnection();
        try {
            pqxx::work retryTxn(conn);
            pqxx::result r = retryTxn.exec_params("SELECT id_users FROM users WHERE email = $1", email);
            return !r.empty();
        } catch (...) {
            throw; // laisser l'appelant gérer
        }
    }
}

void Database::registerUser(const std::string& fullName, const std::string& email, const std::string& hashedPassword) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params(
            "INSERT INTO users (full_name, email, password_hash) VALUES ($1, $2, $3)",
            fullName, email, hashedPassword
        );
        txn.commit();
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in registerUser: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        retryTxn.exec_params(
            "INSERT INTO users (full_name, email, password_hash) VALUES ($1, $2, $3)",
            fullName, email, hashedPassword
        );
        retryTxn.commit();
    }
}

std::optional<UserRecord> Database::getUserByEmail(const std::string& email) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT u.id_users, u.email, u.password_hash, u.full_name, u.role_id, r.name as role_name "
            "FROM users u LEFT JOIN roles r ON u.role_id = r.id_roles WHERE u.email = $1", 
            email);

        if (r.empty()) return std::nullopt;

        UserRecord user{
            r[0]["id_users"].as<int>(),
            r[0]["email"].as<std::string>(),
            r[0]["password_hash"].as<std::string>(),
            r[0]["full_name"].as<std::string>(),
            r[0]["role_id"].is_null() ? 0 : r[0]["role_id"].as<int>(),
            r[0]["role_name"].is_null() ? "" : r[0]["role_name"].as<std::string>()
        };
        return user;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in getUserByEmail: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        pqxx::result r = retryTxn.exec_params(
            "SELECT u.id_users, u.email, u.password_hash, u.full_name, u.role_id, r.name as role_name "
            "FROM users u LEFT JOIN roles r ON u.role_id = r.id_roles WHERE u.email = $1", 
            email);
        if (r.empty()) return std::nullopt;
        UserRecord user{
            r[0]["id_users"].as<int>(),
            r[0]["email"].as<std::string>(),
            r[0]["password_hash"].as<std::string>(),
            r[0]["full_name"].as<std::string>(),
            r[0]["role_id"].is_null() ? 0 : r[0]["role_id"].as<int>(),
            r[0]["role_name"].is_null() ? "" : r[0]["role_name"].as<std::string>()
        };
        return user;
    }
}

// RBAC Methods Implementation
std::vector<std::string> Database::getUserPermissions(int user_id) {
    ensureConnection();
    std::vector<std::string> permissions;
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT DISTINCT p.name FROM users u "
            "JOIN roles r ON u.role_id = r.id_roles "
            "JOIN role_permission rp ON r.id_roles = rp.id_roles "
            "JOIN permissions p ON rp.id_permissions = p.id_permissions "
            "WHERE u.id_users = $1", 
            user_id);
        
        for (const auto& row : r) {
            permissions.push_back(row["name"].as<std::string>());
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error getting user permissions: " << e.what() << std::endl;
    }
    return permissions;
}

bool Database::isUserAdmin(const std::string& email) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT r.name FROM users u "
            "JOIN roles r ON u.role_id = r.id_roles "
            "WHERE u.email = $1", 
            email);
        
        if (!r.empty()) {
            std::string role_name = r[0]["name"].as<std::string>();
            return role_name == "admin";
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error checking admin status: " << e.what() << std::endl;
    }
    return false;
}

bool Database::assignRoleToUser(int user_id, int role_id) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params("UPDATE users SET role_id = $1 WHERE id_users = $2", role_id, user_id);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error assigning role: " << e.what() << std::endl;
        return false;
    }
}

RoleRecord Database::getRole(int role_id) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT id_roles, name, description FROM roles WHERE id_roles = $1", 
            role_id);
        
        if (!r.empty()) {
            RoleRecord role;
            role.id = r[0]["id_roles"].as<int>();
            role.name = r[0]["name"].as<std::string>();
            role.description = r[0]["description"].as<std::string>();
            
            // Get permissions for this role
            pqxx::result perm_r = txn.exec_params(
                "SELECT p.name FROM role_permission rp "
                "JOIN permissions p ON rp.id_permissions = p.id_permissions "
                "WHERE rp.id_roles = $1", 
                role_id);
            
            for (const auto& prow : perm_r) {
                role.permissions.push_back(prow["name"].as<std::string>());
            }
            
            return role;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error getting role: " << e.what() << std::endl;
    }
    return RoleRecord{0, "", "", {}};
}

bool Database::createRole(const std::string& name, const std::string& description) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params(
            "INSERT INTO roles (name, description) VALUES ($1, $2)", 
            name, description);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error creating role: " << e.what() << std::endl;
        return false;
    }
}

std::vector<RoleRecord> Database::listRoles() {
    ensureConnection();
    std::vector<RoleRecord> roles;
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec("SELECT id_roles, name, description FROM roles ORDER BY name");
        
        for (const auto& row : r) {
            RoleRecord role;
            role.id = row["id_roles"].as<int>();
            role.name = row["name"].as<std::string>();
            role.description = row["description"].as<std::string>();
            
            // Get permissions for each role
            int role_id = role.id;
            pqxx::result perm_r = txn.exec_params(
                "SELECT p.name FROM role_permission rp "
                "JOIN permissions p ON rp.id_permissions = p.id_permissions "
                "WHERE rp.id_roles = $1", 
                role_id);
            
            for (const auto& prow : perm_r) {
                role.permissions.push_back(prow["name"].as<std::string>());
            }
            
            roles.push_back(role);
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error listing roles: " << e.what() << std::endl;
    }
    return roles;
}

bool Database::addPermissionToRole(int role_id, const std::string& permission) {
    ensureConnection();
    try {
        pqxx::work txn(conn);
        
        // First get permission ID
        pqxx::result perm_r = txn.exec_params(
            "SELECT id_permissions FROM permissions WHERE name = $1", 
            permission);
        
        if (perm_r.empty()) {
            std::cerr << "[Database] Permission not found: " << permission << std::endl;
            return false;
        }
        
        int permission_id = perm_r[0]["id_permissions"].as<int>();
        
        // Add to role_permission if not exists
        txn.exec_params(
            "INSERT INTO role_permission (id_roles, id_permissions) "
            "SELECT $1, $2 WHERE NOT EXISTS ("
            "SELECT 1 FROM role_permission WHERE id_roles = $1 AND id_permissions = $2)", 
            role_id, permission_id);
        
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error adding permission to role: " << e.what() << std::endl;
        return false;
    }
}
