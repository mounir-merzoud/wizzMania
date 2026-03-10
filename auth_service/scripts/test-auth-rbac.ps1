param(
  [string]$GrpcurlDir = "C:\Users\lo\Downloads\grpcurl_1.9.3_windows_x86_64",
  [string]$Server = "localhost:50061",
  [string]$AdminUsername = "admin@wizzmania.com",
  [string]$AdminPassword = "AdminPass123!",
  [string]$RoleName = "auditor",
  [string]$RoleDescription = "Audit / lecture seule",
  [string[]]$RolePermissions = @("user.read"),
  [string]$TargetUserEmail = ""
)

function Get-GrpcurlPath {
  param([string]$Dir)
  if ($Dir -and (Test-Path $Dir)) {
    $match = Get-ChildItem -File -Filter "grpcurl*.exe" -Path $Dir -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($match) { return $match.FullName }
  }
  $cmd = Get-Command grpcurl -ErrorAction SilentlyContinue
  if ($cmd) { return $cmd.Source }
  throw "grpcurl.exe introuvable. Spécifie -GrpcurlDir ou ajoute grpcurl au PATH."
}

try {
  $grpcurl = Get-GrpcurlPath -Dir $GrpcurlDir
  Write-Host "grpcurl: $grpcurl"

  $protoPath = Resolve-Path (Join-Path $PSScriptRoot "..\proto\auth.proto")
  Write-Host "proto:   $protoPath"
  Write-Host "host:    $Server"

  # Login admin (username == email dans le service)
  $loginReq = @{ username = $AdminUsername; password = $AdminPassword } | ConvertTo-Json -Compress
  Write-Host "`nCalling Login..."
  Write-Host "Request: $loginReq"
  
  $loginJson = & $grpcurl -plaintext -proto $protoPath -d $loginReq $Server "securecloud.auth.AuthService/Login"
  Write-Host "Response: $loginJson"
  
  $login = $loginJson | ConvertFrom-Json
  if (-not $login.access_token) { throw "Login n'a pas retourné access_token." }
  $accessToken = $login.access_token
  $adminToken = $accessToken

  # ValidateToken
  $validateReq = @{ access_token = $accessToken } | ConvertTo-Json -Compress
  Write-Host "`nCalling ValidateToken..."
  $validateJson = & $grpcurl -plaintext -proto $protoPath -d $validateReq $Server "securecloud.auth.AuthService/ValidateToken"
  Write-Host "Response: $validateJson"
  $validate = $validateJson | ConvertFrom-Json

  if (-not $validate.valid) { throw "Token invalide — vérifie que l'admin existe et que JWT_SECRET est stable." }

  # CreateRole
  $createRoleReq = @{
    admin_token  = $adminToken
    role_name    = $RoleName
    description  = $RoleDescription
    permissions  = $RolePermissions
  } | ConvertTo-Json -Compress
  Write-Host "`nCalling CreateRole..."
  Write-Host "Request: $createRoleReq"
  $createRoleJson = & $grpcurl -plaintext -proto $protoPath -d $createRoleReq $Server "securecloud.auth.AuthService/CreateRole"
  Write-Host "Response: $createRoleJson"

  # ListRoles
  $listRolesReq = @{ admin_token = $adminToken } | ConvertTo-Json -Compress
  Write-Host "`nCalling ListRoles..."
  $listRolesJson = & $grpcurl -plaintext -proto $protoPath -d $listRolesReq $Server "securecloud.auth.AuthService/ListRoles"
  Write-Host "Response: $listRolesJson"

  # AssignRole (optionnel)
  if ($TargetUserEmail -and $TargetUserEmail.Trim().Length -gt 0) {
    $assignRoleReq = @{ admin_token = $adminToken; user_email = $TargetUserEmail; role_name = $RoleName } | ConvertTo-Json -Compress
    Write-Host "`nCalling AssignRole..."
    Write-Host "Request: $assignRoleReq"
    $assignRoleJson = & $grpcurl -plaintext -proto $protoPath -d $assignRoleReq $Server "securecloud.auth.AuthService/AssignRole"
    Write-Host "Response: $assignRoleJson"

    # GetUserPermissions
    $getPermsReq = @{ admin_token = $adminToken; user_email = $TargetUserEmail } | ConvertTo-Json -Compress
    Write-Host "`nCalling GetUserPermissions..."
    Write-Host "Request: $getPermsReq"
    $getPermsJson = & $grpcurl -plaintext -proto $protoPath -d $getPermsReq $Server "securecloud.auth.AuthService/GetUserPermissions"
    Write-Host "Response: $getPermsJson"
  } else {
    Write-Host "`n(Skipping AssignRole/GetUserPermissions: -TargetUserEmail vide)"
  }

  Write-Host "`n--- Résumé ---"
  Write-Host ("Access Token: {0}" -f $accessToken.Substring(0, 20) + "...")
  Write-Host ("Valid:        {0}" -f $validate.valid)
  if ($validate.user_id) { Write-Host ("User ID:      {0}" -f $validate.user_id) }
  if ($validate.email) { Write-Host ("Email:        {0}" -f $validate.email) }
  if ($validate.role) { Write-Host ("Role:         {0}" -f $validate.role) }
  if ($validate.permissions) { Write-Host ("Permissions:  {0}" -f ($validate.permissions -join ", ")) }
  if ($login.role) { Write-Host ("Login Role:   {0}" -f $login.role) }
  if ($login.permissions) { Write-Host ("Login Perms:  {0}" -f ($login.permissions -join ", ")) }

  exit 0
}
catch {
  Write-Error $_.Exception.Message
  exit 1
}