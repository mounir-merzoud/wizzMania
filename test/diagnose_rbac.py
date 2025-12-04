#!/usr/bin/env python3
import psycopg2
import os

# Configuration de connexion
conn_str = "postgresql://neondb_owner:npg_xQce1PdSwzs2@ep-bold-credit-agqg5ma3-pooler.c-2.eu-central-1.aws.neon.tech/neondb?sslmode=require&channel_binding=require"

try:
    conn = psycopg2.connect(conn_str)
    cur = conn.cursor()
    
    print("🔍 État de la base de données RBAC:")
    print("=" * 50)
    
    # Vérifier les rôles
    cur.execute("SELECT id_roles, name, description FROM roles ORDER BY id_roles")
    roles = cur.fetchall()
    print(f"\n📋 Rôles ({len(roles)}):")
    for role in roles:
        print(f"  - ID: {role[0]}, Nom: {role[1]}, Description: {role[2]}")
    
    # Vérifier les permissions  
    cur.execute("SELECT id_permissions, name, description FROM permissions ORDER BY id_permissions")
    permissions = cur.fetchall()
    print(f"\n🔑 Permissions ({len(permissions)}):")
    for perm in permissions:
        print(f"  - ID: {perm[0]}, Nom: {perm[1]}, Description: {perm[2]}")
    
    # Vérifier les utilisateurs
    cur.execute("""
        SELECT u.id_users, u.email, u.role_id, r.name as role_name 
        FROM users u 
        LEFT JOIN roles r ON u.role_id = r.id_roles 
        ORDER BY u.id_users
    """)
    users = cur.fetchall()
    print(f"\n👥 Utilisateurs ({len(users)}):")
    for user in users:
        role_info = f"Rôle: {user[3]}" if user[3] else "❌ AUCUN RÔLE"
        print(f"  - ID: {user[0]}, Email: {user[1]}, Role_ID: {user[2]}, {role_info}")
    
    # Vérifier les associations rôle-permission
    cur.execute("""
        SELECT r.name as role_name, p.name as permission_name
        FROM role_permission rp
        JOIN roles r ON rp.id_roles = r.id_roles  
        JOIN permissions p ON rp.id_permissions = p.id_permissions
        ORDER BY r.name, p.name
    """)
    role_perms = cur.fetchall()
    print(f"\n🔗 Associations Rôle-Permission ({len(role_perms)}):")
    current_role = None
    for rp in role_perms:
        if rp[0] != current_role:
            current_role = rp[0]
            print(f"\n  {current_role}:")
        print(f"    - {rp[1]}")
    
    cur.close()
    conn.close()
    
    print(f"\n✅ Diagnostic terminé")
    
except Exception as e:
    print(f"❌ Erreur: {e}")