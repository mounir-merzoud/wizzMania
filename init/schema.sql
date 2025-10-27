-- ============================================
-- SCHEMA SECURECLOUD - Initialisation PostgreSQL
-- ============================================

-- 🔹 TABLE: roles
CREATE TABLE roles (
    id_roles SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT
);

-- 🔹 TABLE: permissions
CREATE TABLE permissions (
    id_permissions SERIAL PRIMARY KEY,
    name VARCHAR(100) NOT NULL UNIQUE,
    description TEXT
);

-- 🔹 TABLE: role_permission (association N-N)
CREATE TABLE role_permission (
    id_roles INT NOT NULL,
    id_permissions INT NOT NULL,
    PRIMARY KEY (id_roles, id_permissions),
    FOREIGN KEY (id_roles) REFERENCES roles(id_roles) ON DELETE CASCADE,
    FOREIGN KEY (id_permissions) REFERENCES permissions(id_permissions) ON DELETE CASCADE
);

-- 🔹 TABLE: users
CREATE TABLE users (
    id_users SERIAL PRIMARY KEY,
    full_name VARCHAR(150) NOT NULL,
    email VARCHAR(150) NOT NULL UNIQUE,
    password_hash TEXT NOT NULL,
    public_key TEXT,
    role_id INT REFERENCES roles(id_roles) ON DELETE SET NULL,
    status VARCHAR(50) DEFAULT 'active',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    token_jti TEXT
);

-- 🔹 TABLE: session
CREATE TABLE session (
    id_session SERIAL PRIMARY KEY,
    user_id INT REFERENCES users(id_users) ON DELETE CASCADE,
    token_jti TEXT,
    ip_address VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    expires_at TIMESTAMP
);

-- 🔹 TABLE: logs_audit
CREATE TABLE logs_audit (
    id_logs_audit SERIAL PRIMARY KEY,
    user_id INT REFERENCES users(id_users) ON DELETE SET NULL,
    action_type VARCHAR(100),
    service_name VARCHAR(100),
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    encrypted_log TEXT
);

-- 🔹 TABLE: alerts
CREATE TABLE alerts (
    id_alerts SERIAL PRIMARY KEY,
    user_id INT REFERENCES users(id_users) ON DELETE CASCADE,
    alert_type VARCHAR(100),
    description TEXT,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    resolved BOOLEAN DEFAULT FALSE
);

-- 🔹 TABLE: files
CREATE TABLE files (
    id_files SERIAL PRIMARY KEY,
    owner_id INT REFERENCES users(id_users) ON DELETE CASCADE,
    filename VARCHAR(255),
    hash_sha256 TEXT,
    encrypted_path TEXT,
    size_mb NUMERIC(10,2),
    uploaded_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
    id_logs_audit INT REFERENCES logs_audit(id_logs_audit)
);

-- 🔹 TABLE: conversations
CREATE TABLE conversations (
    id_conversations SERIAL PRIMARY KEY,
    title VARCHAR(150),
    type VARCHAR(50) DEFAULT 'private',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 🔹 TABLE: conversation_participant (association N-N)
CREATE TABLE conversation_participant (
    id_users INT REFERENCES users(id_users) ON DELETE CASCADE,
    id_conversations INT REFERENCES conversations(id_conversations) ON DELETE CASCADE,
    PRIMARY KEY (id_users, id_conversations)
);

-- 🔹 TABLE: messages
CREATE TABLE messages (
    id_messages SERIAL PRIMARY KEY,
    conversation_id INT REFERENCES conversations(id_conversations) ON DELETE CASCADE,
    sender_id INT REFERENCES users(id_users) ON DELETE SET NULL,
    encrypted_content TEXT NOT NULL,
    priority VARCHAR(50) DEFAULT 'normal',
    status VARCHAR(50) DEFAULT 'sent',
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- 🔹 TABLE: tokens_revoked
CREATE TABLE tokens_revoked (
    token_jti TEXT PRIMARY KEY,
    revoked_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);

-- ============================================
-- ✅ FIN DU SCHEMA SECURECLOUD
-- ============================================
