param(
  [string]$GrpcurlDir = "C:\Users\lo\Downloads\grpcurl_1.9.3_windows_x86_64",
  [string]$Server = "localhost:50061",
  [string]$Email = "admin@wizzmania.com",
  [string]$Password = "AdminPass123!"
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

  # Login avec email/password
  $loginReq = @{ email = $Email; password = $Password } | ConvertTo-Json -Compress
  Write-Host "`nCalling Login..."
  Write-Host "Request: $loginReq"
  
  $loginJson = & $grpcurl -plaintext -import-path (Split-Path $protoPath) -proto (Split-Path $protoPath -Leaf) -d $loginReq $Server "auth.AuthService/Login"
  Write-Host "Response: $loginJson"
  
  $login = $loginJson | ConvertFrom-Json
  if (-not $login.access_token) { throw "Login n'a pas retourné access_token." }
  $accessToken = $login.access_token

  # ValidateToken
  $validateReq = @{ access_token = $accessToken } | ConvertTo-Json -Compress
  Write-Host "`nCalling ValidateToken..."
  $validateJson = & $grpcurl -plaintext -import-path (Split-Path $protoPath) -proto (Split-Path $protoPath -Leaf) -d $validateReq $Server "auth.AuthService/ValidateToken"
  Write-Host "Response: $validateJson"
  $validate = $validateJson | ConvertFrom-Json

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