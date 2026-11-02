$ErrorActionPreference='Stop'
$active=Get-Content (Join-Path $PSScriptRoot 'recordings\active.json') -Raw | ConvertFrom-Json
New-Item -ItemType File -Path (Join-Path $active.directory 'STOP') -Force | Out-Null
Write-Output "Stop requested. Recording: $($active.directory)"
