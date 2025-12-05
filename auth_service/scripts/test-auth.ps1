param(
  [string]$GrpcurlDir = "C:\\Users\\lo\\Downloads\\grpcurl_1.9.3_windows_x86_64",
  [string]$Server = "localhost:50051",
  [string]$Username = "admin",
  [string]$PasswordPlain = "secure"
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

  # Login
  $loginReq = @{ username = $Username; password = $PasswordPlain } | ConvertTo-Json -Compress
  Write-Host "\nCalling Login..."
  $loginJson = & $grpcurl -plaintext -proto $protoPath -d $loginReq $Server "securecloud.auth.AuthService/Login"
  Write-Host $loginJson
  $login = $loginJson | ConvertFrom-Json
  if (-not $login.access_token) { throw "Login n'a pas retourné access_token." }
  $accessToken = $login.access_token

  # ValidateToken
  $validateReq = @{ access_token = $accessToken } | ConvertTo-Json -Compress
  Write-Host "\nCalling ValidateToken..."
  $validateJson = & $grpcurl -plaintext -proto $protoPath -d $validateReq $Server "securecloud.auth.AuthService/ValidateToken"
  Write-Host $validateJson
  $validate = $validateJson | ConvertFrom-Json

  Write-Host "\n--- Résumé ---"
  Write-Host ("Access Token: {0}" -f $accessToken)
  Write-Host ("Valid:        {0}" -f $validate.valid)
  if ($validate.user_id) { Write-Host ("User ID:      {0}" -f $validate.user_id) }
  if ($validate.permissions) { Write-Host ("Permissions:  {0}" -f ($validate.permissions -join ", ")) }

  exit 0
}
catch {
  Write-Error $_
  exit 1
}
