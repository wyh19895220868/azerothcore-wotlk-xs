param(
    [Parameter(Mandatory = $true)]
    [string]$ClientSourceRoot
)

$ErrorActionPreference = 'Stop'
$serverHeader = Join-Path $PSScriptRoot '../src/common/Utilities/NativeUIProtocol.h'
$clientHeader = Join-Path $ClientSourceRoot 'WoW/Source/NativeUIProtocol.h'
$serverHash = (Get-FileHash -LiteralPath $serverHeader -Algorithm SHA256).Hash
$clientHash = (Get-FileHash -LiteralPath $clientHeader -Algorithm SHA256).Hash
if ($serverHash -ne $clientHash) {
    throw 'NativeUI protocol constants differ between server and client sources.'
}
Write-Output 'PASS: server and client NativeUI protocol constants match.'
