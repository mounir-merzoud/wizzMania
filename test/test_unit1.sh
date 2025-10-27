#!/bin/bash


set -e


RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' 


PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TEST_DIR="$PROJECT_DIR/test"
MESSAGING_SERVICE_PID=""
AUTH_SERVICE_PID=""
TEST_CLIENT_SRC="$PROJECT_DIR/services/messaging_service/src/test_client.cpp"
MESSAGING_DIR="$PROJECT_DIR/services/messaging_service"

echo -e "${BLUE}🧙‍♂️ wizzMania - Suite de tests complète${NC}"
echo -e "${BLUE}===============================================${NC}"


cleanup() {
    echo -e "\n${YELLOW}🧹 Nettoyage...${NC}"
    if [ ! -z "$MESSAGING_SERVICE_PID" ]; then
        kill $MESSAGING_SERVICE_PID 2>/dev/null || true
        echo -e "${GREEN}✅ Service de messagerie arrêté${NC}"
    fi
    if [ ! -z "$AUTH_SERVICE_PID" ]; then
        kill $AUTH_SERVICE_PID 2>/dev/null || true
        echo -e "${GREEN}✅ Service d'authentification arrêté${NC}"
    fi
}


trap cleanup EXIT


cd "$PROJECT_DIR"
echo -e "${YELLOW}🔧 Lancement du serveur de messaging${NC}"

if make build-messaging > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Service de messagerie compilé${NC}"
else
    echo -e "${RED}❌ Échec de compilation du service de messagerie${NC}"
    exit 1
fi

if [ ! -f "$MESSAGING_DIR/build/messaging_service" ]; then
    echo -e "${RED}❌ Exécutable messaging_service non trouvé${NC}"
    exit 1
fi

cd "$MESSAGING_DIR"
echo -e "${YELLOW}🚀 Démarrage du service de messagerie...${NC}"

./build/messaging_service &
MESSAGING_SERVICE_PID=$!

sleep 3


if kill -0 $MESSAGING_SERVICE_PID 2>/dev/null; then
    echo -e "${GREEN}✅ Service de messagerie démarré (PID: $MESSAGING_SERVICE_PID)${NC}"
else
    echo -e "${RED}❌ Échec du démarrage du service de messagerie${NC}"
    exit 1
fi


echo -e "${YELLOW}🏥 Test de santé du service de messagerie...${NC}"
sleep 2 

if nc -z localhost 7002 2>/dev/null; then
    echo -e "${GREEN}✅ Service de messagerie répond sur le port 7002${NC}"
else
    echo -e "${RED}❌ Service de messagerie ne répond pas sur le port 7002${NC}"
    exit 1
fi
