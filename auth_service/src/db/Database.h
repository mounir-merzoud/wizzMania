#pragma once
#include <pqxx/pqxx>
#include <optional>
#include <string>

struct UserRecord {
    int id;                 // maps to users.id_users
    std::string email;      // users.email
    std::string password_hash; // users.password_hash
};

class Database {
private:
    pqxx::connection conn;

public:
    explicit Database(const std::string& connStr);
    bool userExists(const std::string& email);
    void registerUser(const std::string& fullName, const std::string& email, const std::string& hashedPassword);
    std::optional<UserRecord> getUserByEmail(const std::string& email);
};
