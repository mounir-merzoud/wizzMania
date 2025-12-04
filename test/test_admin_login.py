#!/usr/bin/env python3
import grpc
import sys
import os
sys.path.append(os.path.join(os.path.dirname(__file__), '..', 'auth_service', 'generated'))

import auth_pb2
import auth_pb2_grpc

def test_admin_login():
    # Connexion au service gRPC
    channel = grpc.insecure_channel('localhost:50061')
    stub = auth_pb2_grpc.AuthServiceStub(channel)
    
    print("🔐 Test de connexion admin...")
    
    # Test de connexion admin
    login_request = auth_pb2.LoginRequest(
        email="admin@wizzmania.com",
        password="AdminPass123!"
    )
    
    try:
        response = stub.Login(login_request)
        print(f"✅ Connexion admin réussie !")
        print(f"🎫 Token généré: {response.access_token[:50]}...")
        print(f"👤 Rôle: '{response.role}'")
        print(f"🔑 Permissions ({len(response.permissions)}): {list(response.permissions)}")
        
        if response.role:
            print(f"✅ Rôle correctement récupéré: {response.role}")
        else:
            print("⚠️  Rôle vide - problème RBAC détecté")
            
        if response.permissions:
            print(f"✅ Permissions correctement récupérées: {len(response.permissions)} permissions")
        else:
            print("⚠️  Permissions vides - problème RBAC détecté")
            
        return response.access_token
        
    except grpc.RpcError as e:
        print(f"❌ Erreur de connexion: {e.code()} - {e.details()}")
        return None

if __name__ == "__main__":
    token = test_admin_login()
    if token:
        print(f"\n🎯 Token admin disponible pour les tests suivants")
    else:
        print("\n❌ Impossible d'obtenir le token admin")