param(
    [string]$ServerPort = "50061"
)

Write-Host "🔐 Test de connexion admin RBAC" -ForegroundColor Green
Write-Host "📍 Serveur: localhost:$ServerPort" -ForegroundColor Blue

# Test de connexion admin
$loginData = @{
    email = "admin@wizzmania.com"
    password = "AdminPass123!"
} | ConvertTo-Json

Write-Host "📤 Envoi de la requête de connexion..." -ForegroundColor Yellow

try {
    # Utiliser Invoke-RestMethod pour tester (si le service expose REST)
    # Sinon, on utilise une approche différente
    Write-Host "⚠️  Ce script nécessite grpcurl ou un client gRPC pour tester" -ForegroundColor Red
    Write-Host "💡 Vérifiez manuellement que le service répond sur le port $ServerPort" -ForegroundColor Cyan
    
    # Test de connectivité basique
    $connection = Test-NetConnection -ComputerName localhost -Port $ServerPort -WarningAction SilentlyContinue
    if ($connection.TcpTestSucceeded) {
        Write-Host "✅ Service accessible sur le port $ServerPort" -ForegroundColor Green
        Write-Host "📋 Données de test:" -ForegroundColor Cyan
        Write-Host "   Email: admin@wizzmania.com" -ForegroundColor White
        Write-Host "   Password: AdminPass123!" -ForegroundColor White
        Write-Host "🎯 Le service est prêt pour les tests avec un client gRPC" -ForegroundColor Green
    } else {
        Write-Host "❌ Service non accessible sur le port $ServerPort" -ForegroundColor Red
    }
    
} catch {
    Write-Host "❌ Erreur: $_" -ForegroundColor Red
}