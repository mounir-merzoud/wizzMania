#include "Database.h"
#include <iostream>

Database::Database(const std::string& connStr)
    : conn(connStr) {}

bool Database::userExists(const std::string& email) {
    pqxx::work txn(conn);
    pqxx::result r = txn.exec_params("SELECT id_users FROM users WHERE email = $1", email);
    return !r.empty();
}

void Database::registerUser(const std::string& fullName, const std::string& email, const std::string& hashedPassword) {
    pqxx::work txn(conn);
    txn.exec_params(
        "INSERT INTO users (full_name, email, password_hash) VALUES ($1, $2, $3)",
        fullName, email, hashedPassword
    );
    txn.commit();
}

std::optional<UserRecord> Database::getUserByEmail(const std::string& email) {
    pqxx::work txn(conn);
    pqxx::result r = txn.exec_params("SELECT id_users, email, password_hash FROM users WHERE email = $1", email);

    if (r.empty()) return std::nullopt;

    UserRecord user{
        r[0]["id_users"].as<int>(),
        r[0]["email"].as<std::string>(),
        r[0]["password_hash"].as<std::string>()
    };
    return user;
}
