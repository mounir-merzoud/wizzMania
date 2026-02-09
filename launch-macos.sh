#!/bin/bash

echo "🚀 Lancement WizzMania sur macOS"
echo ""

# Terminal 1 : Auth Service (Docker)
echo "✅ Auth Service déjà lancé dans Docker (port 50051)"

# Terminal 2 : Gateway Service
echo "🌐 Lancement Gateway Service..."
cd /Users/mathisserra/Desktop/Github/B3_Laplateforme/wizzMania/gateway_service
./build/gateway_service &
GATEWAY_PID=$!
echo "✅ Gateway lancé (PID: $GATEWAY_PID)"

sleep 2

# Terminal 3 : Frontend Qt
echo "🖥️  Lancement Frontend..."
cd /Users/mathisserra/Desktop/Github/B3_Laplateforme/wizzMania
open build/wizzMania.app

echo ""
echo "✅ Tout est lancé !"
echo ""
echo "📊 Services :"
echo "  - Auth Service (Docker): http://localhost:50051"
echo "  - Gateway Service: http://localhost:8080"
echo "  - Frontend: Application MSF"
echo ""
echo "🛑 Pour arrêter le gateway: kill $GATEWAY_PID"
echo "   Ou: pkill gateway_service"
