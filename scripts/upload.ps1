param([string]$HostName = "http://iotsupport.iotsupport.svc.cluster.local")

$ErrorActionPreference = "Stop"

Push-Location (Split-Path $PSScriptRoot -Parent)

try {
    Invoke-RestMethod "$HostName/api/pipeline/upload.ps1" | Invoke-Expression
    Upload-Firmware
} finally {
    Pop-Location
}
