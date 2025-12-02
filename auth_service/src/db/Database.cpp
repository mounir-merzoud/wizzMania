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
        pqxx::result r = txn.exec_params("SELECT id_users, email, password_hash FROM users WHERE email = $1", email);

        if (r.empty()) return std::nullopt;

        UserRecord user{
            r[0]["id_users"].as<int>(),
            r[0]["email"].as<std::string>(),
            r[0]["password_hash"].as<std::string>()
        };
        return user;
    } catch (const pqxx::broken_connection& bc) {
        std::cerr << "[Database] broken_connection in getUserByEmail: " << bc.what() << std::endl;
        ensureConnection();
        pqxx::work retryTxn(conn);
        pqxx::result r = retryTxn.exec_params("SELECT id_users, email, password_hash FROM users WHERE email = $1", email);
        if (r.empty()) return std::nullopt;
        UserRecord user{
            r[0]["id_users"].as<int>(),
            r[0]["email"].as<std::string>(),
            r[0]["password_hash"].as<std::string>()
        };
        return user;
    }
}
