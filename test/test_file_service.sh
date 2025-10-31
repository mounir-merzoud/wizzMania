#!/bin/bash

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FILES_DIR="$PROJECT_DIR/services/file_service"
BINARY_SERVER="$FILES_DIR/build/file_service"
BINARY_CLIENT="$FILES_DIR/build/file_service_test_client"
FILE_SERVICE_PID=""
STORAGE_DIR=""
SERVER_ADDR=""

cleanup() {
    if [[ -n "$FILE_SERVICE_PID" ]] && kill -0 "$FILE_SERVICE_PID" 2>/dev/null; then
        echo -e "${YELLOW}🧹 Arrêt du service de fichiers...${NC}"
        kill "$FILE_SERVICE_PID" 2>/dev/null || true
        wait "$FILE_SERVICE_PID" 2>/dev/null || true
    fi
    if [[ -n "$STORAGE_DIR" && -d "$STORAGE_DIR" ]]; then
        rm -rf "$STORAGE_DIR"
    fi
}

trap cleanup EXIT

echo -e "${BLUE}🧪 Test unitaire - FileService${NC}"

echo -e "${YELLOW}🔧 Compilation du service de fichiers...${NC}"
if make build-files -C "$PROJECT_DIR" > /dev/null 2>&1; then
    echo -e "${GREEN}✅ Build file_service OK${NC}"
else
    echo -e "${RED}❌ Échec de compilation du service de fichiers${NC}"
    exit 1
fi

if [[ ! -x "$BINARY_SERVER" ]]; then
    echo -e "${RED}❌ Binaire file_service introuvable${NC}"
    exit 1
fi

if [[ ! -x "$BINARY_CLIENT" ]]; then
    echo -e "${RED}❌ Binaire file_service_test_client introuvable${NC}"
    exit 1
fi

STORAGE_DIR="$(mktemp -d 2>/dev/null || mktemp -d -t file_service_storage)"

if command -v python3 >/dev/null 2>&1; then
    FILE_SERVICE_PORT="$(python3 - <<'PY'
import socket
with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
    s.bind(('127.0.0.1', 0))
    print(s.getsockname()[1])
PY
)"
else
    # Best-effort fallback port when python3 is unavailable
    FILE_SERVICE_PORT="$(( (RANDOM % 1000) + 55000 ))"
fi

SERVER_ADDR="127.0.0.1:${FILE_SERVICE_PORT}"

echo -e "${YELLOW}📡 Utilisation de l'adresse de test ${SERVER_ADDR}${NC}"

cd "$FILES_DIR/build"
echo -e "${YELLOW}🚀 Démarrage du service de fichiers...${NC}"
FILE_SERVICE_STORAGE="$STORAGE_DIR" ./file_service "$SERVER_ADDR" &
FILE_SERVICE_PID=$!

sleep 2

if ! kill -0 "$FILE_SERVICE_PID" 2>/dev/null; then
    echo -e "${RED}❌ Service de fichiers introuvable ou arrêté${NC}"
    exit 1
fi

echo -e "${YELLOW}🧪 Exécution du client de test...${NC}"
if ./file_service_test_client "$SERVER_ADDR"; then
    echo -e "${GREEN}✅ Test d'upload/download/suppression réussi${NC}"
else
    echo -e "${RED}❌ Le client de test a échoué${NC}"
    exit 1
fi

echo -e "${GREEN}🎉 Tous les tests FileService ont réussi${NC}"
