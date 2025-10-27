#!/bin/bash

set -e

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
AUTH_DIR="$PROJECT_DIR/services/auth_service"
AUTH_BIN="$AUTH_DIR/build/auth_service"
TEST_CLIENT_BIN="$AUTH_DIR/build/auth_test_client"
AUTH_PID=""

cleanup() {
    if [ -n "$AUTH_PID" ] && kill -0 "$AUTH_PID" 2>/dev/null; then
        echo -e "${YELLOW}🧹 Arrêt du service d'authentification...${NC}"
        kill "$AUTH_PID" 2>/dev/null || true
        wait "$AUTH_PID" 2>/dev/null || true
        echo -e "${GREEN}✅ Service d'authentification arrêté${NC}"
    fi
}

trap cleanup EXIT

echo -e "${BLUE}🧙‍♂️ Test unitaire - Authentification${NC}"
echo -e "${BLUE}====================================${NC}"

cd "$PROJECT_DIR"
echo -e "${YELLOW}🔧 Compilation du service d'authentification...${NC}"
if make build-auth > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Build auth OK${NC}"
else
    echo -e "${RED}❌ Échec de compilation du service d'authentification${NC}"
    exit 1
fi

if [ ! -f "$AUTH_BIN" ]; then
    echo -e "${RED}❌ Binaire auth_service introuvable${NC}"
    exit 1
fi

if [ ! -f "$TEST_CLIENT_BIN" ]; then
    echo -e "${RED}❌ Binaire auth_test_client introuvable${NC}"
    exit 1
fi

cd "$AUTH_DIR"
echo -e "${YELLOW}🚀 Démarrage du service d'authentification...${NC}"
"$AUTH_BIN" &
AUTH_PID=$!

sleep 2

if kill -0 "$AUTH_PID" 2>/dev/null; then
    echo -e "${GREEN}✅ Service d'authentification démarré (PID: $AUTH_PID)${NC}"
else
    echo -e "${RED}❌ Échec du lancement du service d'authentification${NC}"
    exit 1
fi

if command -v nc > /dev/null 2>&1; then
    PORT_READY=0
    for i in 1 2 3 4 5; do
        if nc -z localhost 50051 2>/dev/null; then
            PORT_READY=1
            break
        fi
        sleep 1
    done
    if [ "$PORT_READY" -eq 1 ]; then
        echo -e "${GREEN}✅ Port 50051 joignable${NC}"
    else
        echo -e "${RED}❌ Le port 50051 ne répond pas${NC}"
        exit 1
    fi
else
    echo -e "${YELLOW}⚠️ nc indisponible, saut du test de port${NC}"
fi

cd "$AUTH_DIR/build"
echo -e "${YELLOW}🧪 Exécution du client de test d'authentification...${NC}"
if "$TEST_CLIENT_BIN"; then
    echo -e "${GREEN}✅ Authentification: scénario de test réussi${NC}"
else
    echo -e "${RED}❌ Le client de test a échoué${NC}"
    exit 1
fi

echo -e "${GREEN}🎉 Tous les tests d'authentification ont réussi${NC}"
