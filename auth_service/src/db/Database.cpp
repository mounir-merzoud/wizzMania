#include "Database.h"
#include <iostream>

Database::Database(const std::string& connStr)
    : connStr_(connStr), conn(connStr) {}

void Database::ensureConnection() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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

void Database::registerUserWithRole(const std::string& fullName, const std::string& email, const std::string& hashedPassword, const std::string& roleName) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        
        // Récupérer l'ID du rôle
        std::cout << "[Database] Recherche du rôle: " << roleName << std::endl;
        pqxx::result role_r = txn.exec_params("SELECT id_roles FROM roles WHERE name = $1", roleName);
        int role_id = 0;
        if (!role_r.empty()) {
            role_id = role_r[0]["id_roles"].as<int>();
            std::cout << "[Database] Rôle trouvé, ID: " << role_id << std::endl;
        } else {
            std::cout << "[Database] ❌ Rôle '" << roleName << "' non trouvé !" << std::endl;
            // Créer le rôle "user" s'il n'existe pas
            if (roleName == "user") {
                txn.exec_params("INSERT INTO roles (name, description) VALUES ('user', 'Utilisateur standard') ON CONFLICT (name) DO NOTHING");
                pqxx::result retry_role_r = txn.exec_params("SELECT id_roles FROM roles WHERE name = $1", roleName);
                if (!retry_role_r.empty()) {
                    role_id = retry_role_r[0]["id_roles"].as<int>();
                    std::cout << "[Database] Rôle 'user' créé avec ID: " << role_id << std::endl;
                }
            }
        }
        
        // Insérer l'utilisateur avec le rôle
        if (role_id > 0) {
            txn.exec_params(
                "INSERT INTO users (full_name, email, password_hash, role_id) VALUES ($1, $2, $3, $4)",
                fullName, email, hashedPassword, role_id
            );
            std::cout << "[Database] Utilisateur inséré avec role_id: " << role_id << std::endl;
        } else {
            txn.exec_params(
                "INSERT INTO users (full_name, email, password_hash) VALUES ($1, $2, $3)",
                fullName, email, hashedPassword
            );
            std::cout << "[Database] Utilisateur inséré SANS rôle (role_id=NULL)" << std::endl;
        }
        txn.commit();
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in registerUserWithRole: " << bc.what() << std::endl;
        ensureConnection();
        // Retry logic similar to above
        pqxx::work retryTxn(conn);
        pqxx::result role_r = retryTxn.exec_params("SELECT id_roles FROM roles WHERE name = $1", roleName);
        int role_id = 0;
        if (!role_r.empty()) {
            role_id = role_r[0]["id_roles"].as<int>();
        }
        
        if (role_id > 0) {
            retryTxn.exec_params(
                "INSERT INTO users (full_name, email, password_hash, role_id) VALUES ($1, $2, $3, $4)",
                fullName, email, hashedPassword, role_id
            );
        } else {
            retryTxn.exec_params(
                "INSERT INTO users (full_name, email, password_hash) VALUES ($1, $2, $3)",
                fullName, email, hashedPassword
            );
        }
        retryTxn.commit();
    }
}

std::optional<UserRecord> Database::getUserByEmail(const std::string& email) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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

std::optional<UserRecord> Database::getUserById(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT u.id_users, u.email, u.password_hash, u.full_name, u.role_id, r.name as role_name "
            "FROM users u LEFT JOIN roles r ON u.role_id = r.id_roles WHERE u.id_users = $1",
            user_id);

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
        std::cerr << "[Database] broken_connection in getUserById: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        pqxx::result r = retryTxn.exec_params(
            "SELECT u.id_users, u.email, u.password_hash, u.full_name, u.role_id, r.name as role_name "
            "FROM users u LEFT JOIN roles r ON u.role_id = r.id_roles WHERE u.id_users = $1",
            user_id);
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

bool Database::deleteUserById(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);

        // Revoke current jti (if any) so existing tokens become invalid.
        pqxx::result jti_r = txn.exec_params(
            "SELECT token_jti FROM users WHERE id_users = $1",
            user_id);
        if (!jti_r.empty() && !jti_r[0]["token_jti"].is_null()) {
            const std::string jti = jti_r[0]["token_jti"].as<std::string>();
            if (!jti.empty()) {
                txn.exec_params(
                    "INSERT INTO tokens_revoked (token_jti) VALUES ($1) ON CONFLICT (token_jti) DO NOTHING",
                    jti);
            }
        }

        pqxx::result r = txn.exec_params(
            "DELETE FROM users WHERE id_users = $1",
            user_id);
        txn.commit();

        return r.affected_rows() > 0;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in deleteUserById: " << bc.what() << std::endl;
        ensureConnection();

        pqxx::work retryTxn(conn);
        pqxx::result jti_r = retryTxn.exec_params(
            "SELECT token_jti FROM users WHERE id_users = $1",
            user_id);
        if (!jti_r.empty() && !jti_r[0]["token_jti"].is_null()) {
            const std::string jti = jti_r[0]["token_jti"].as<std::string>();
            if (!jti.empty()) {
                retryTxn.exec_params(
                    "INSERT INTO tokens_revoked (token_jti) VALUES ($1) ON CONFLICT (token_jti) DO NOTHING",
                    jti);
            }
        }

        pqxx::result r = retryTxn.exec_params(
            "DELETE FROM users WHERE id_users = $1",
            user_id);
        retryTxn.commit();
        return r.affected_rows() > 0;
    }
}

bool Database::updateUserPasswordHash(int user_id, const std::string& new_hashed_password) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params(
            "UPDATE users SET password_hash = $1 WHERE id_users = $2",
            new_hashed_password, user_id);
        txn.commit();
        return true;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in updateUserPasswordHash: " << bc.what() << std::endl;
        ensureConnection();
        try {
            pqxx::work retryTxn(conn);
            retryTxn.exec_params(
                "UPDATE users SET password_hash = $1 WHERE id_users = $2",
                new_hashed_password, user_id);
            retryTxn.commit();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[Database] Error updating password hash: " << e.what() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error updating password hash: " << e.what() << std::endl;
        return false;
    }
}

std::optional<std::string> Database::getUserTokenJti(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT token_jti FROM users WHERE id_users = $1",
            user_id);
        if (r.empty() || r[0]["token_jti"].is_null()) {
            return std::nullopt;
        }
        return r[0]["token_jti"].as<std::string>();
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in getUserTokenJti: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        pqxx::result r = retryTxn.exec_params(
            "SELECT token_jti FROM users WHERE id_users = $1",
            user_id);
        if (r.empty() || r[0]["token_jti"].is_null()) {
            return std::nullopt;
        }
        return r[0]["token_jti"].as<std::string>();
    }
}

bool Database::setUserTokenJti(int user_id, const std::string& token_jti) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        if (token_jti.empty()) {
            txn.exec_params(
                "UPDATE users SET token_jti = NULL WHERE id_users = $1",
                user_id);
        } else {
            txn.exec_params(
                "UPDATE users SET token_jti = $1 WHERE id_users = $2",
                token_jti,
                user_id);
        }
        txn.commit();
        return true;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in setUserTokenJti: " << bc.what() << std::endl;
        ensureConnection();
        try {
            pqxx::work retryTxn(conn);
            if (token_jti.empty()) {
                retryTxn.exec_params(
                    "UPDATE users SET token_jti = NULL WHERE id_users = $1",
                    user_id);
            } else {
                retryTxn.exec_params(
                    "UPDATE users SET token_jti = $1 WHERE id_users = $2",
                    token_jti,
                    user_id);
            }
            retryTxn.commit();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "[Database] Error in setUserTokenJti retry: " << e.what() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error in setUserTokenJti: " << e.what() << std::endl;
        return false;
    }
}

bool Database::isTokenRevoked(const std::string& token_jti) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (token_jti.empty()) return false;
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT token_jti FROM tokens_revoked WHERE token_jti = $1",
            token_jti);
        return !r.empty();
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in isTokenRevoked: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        pqxx::result r = retryTxn.exec_params(
            "SELECT token_jti FROM tokens_revoked WHERE token_jti = $1",
            token_jti);
        return !r.empty();
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error in isTokenRevoked: " << e.what() << std::endl;
        // Fail open would be unsafe; treat as revoked when DB errors.
        return true;
    }
}

bool Database::revokeTokenJti(const std::string& token_jti) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    if (token_jti.empty()) return true;
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params(
            "INSERT INTO tokens_revoked (token_jti) VALUES ($1) ON CONFLICT (token_jti) DO NOTHING",
            token_jti);
        txn.commit();
        return true;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in revokeTokenJti: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        retryTxn.exec_params(
            "INSERT INTO tokens_revoked (token_jti) VALUES ($1) ON CONFLICT (token_jti) DO NOTHING",
            token_jti);
        retryTxn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error in revokeTokenJti: " << e.what() << std::endl;
        return false;
    }
}

std::vector<UserRecord> Database::listUsers() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    std::vector<UserRecord> users;
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec(
            "SELECT u.id_users, u.email, u.password_hash, u.full_name, u.role_id, r.name as role_name "
            "FROM users u LEFT JOIN roles r ON u.role_id = r.id_roles "
            "ORDER BY u.full_name, u.email"
        );

        users.reserve(r.size());
        for (const auto& row : r) {
            users.push_back(UserRecord{
                row["id_users"].as<int>(),
                row["email"].as<std::string>(),
                row["password_hash"].as<std::string>(),
                row["full_name"].as<std::string>(),
                row["role_id"].is_null() ? 0 : row["role_id"].as<int>(),
                row["role_name"].is_null() ? "" : row["role_name"].as<std::string>()
            });
        }
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error listing users: " << e.what() << std::endl;
    }
    return users;
}

// RBAC Methods Implementation
std::vector<std::string> Database::getUserPermissions(int user_id) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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

std::optional<int> Database::getRoleIdByName(const std::string& role_name) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        pqxx::result r = txn.exec_params(
            "SELECT id_roles FROM roles WHERE name = $1",
            role_name);
        if (r.empty()) return std::nullopt;
        return r[0]["id_roles"].as<int>();
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error getting role id by name: " << e.what() << std::endl;
        return std::nullopt;
    }
}

bool Database::createRole(const std::string& name, const std::string& description) {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        txn.exec_params(
            "INSERT INTO roles (name, description) VALUES ($1, $2) ON CONFLICT (name) DO NOTHING",
            name, description);
        txn.commit();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "[Database] Error creating role: " << e.what() << std::endl;
        return false;
    }
}

std::vector<RoleRecord> Database::listRoles() {
    std::lock_guard<std::recursive_mutex> lock(mutex_);
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
    std::lock_guard<std::recursive_mutex> lock(mutex_);
    ensureConnection();
    try {
        pqxx::work txn(conn);
        
        // First get permission ID
        pqxx::result perm_r = txn.exec_params(
            "SELECT id_permissions FROM permissions WHERE name = $1", 
            permission);
        
        if (perm_r.empty()) {
            // Create permission if it does not exist yet.
            txn.exec_params(
                "INSERT INTO permissions (name, description) VALUES ($1, $2) ON CONFLICT (name) DO NOTHING",
                permission, "");
            perm_r = txn.exec_params(
                "SELECT id_permissions FROM permissions WHERE name = $1",
                permission);
            if (perm_r.empty()) {
                std::cerr << "[Database] Permission not found and could not be created: " << permission << std::endl;
                return false;
            }
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
