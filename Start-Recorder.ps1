$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
$active=Get-Process -Name RobloxPlayerBeta -ErrorAction SilentlyContinue
if (@($active).Count -ne 1) { throw 'Exactly one Roblox client must be running.' }
if (Test-Path 'recordings\active.json') {
 $previous=Get-Content 'recordings\active.json' -Raw | ConvertFrom-Json
 $oldProcess=Get-Process -Id $previous.pid -ErrorAction SilentlyContinue
 if($oldProcess -and $oldProcess.ProcessName -eq 'recorder'){throw 'A recorder is already running.'}
}
$session=Join-Path $PSScriptRoot ('recordings\session-'+[guid]::NewGuid().ToString('N').Substring(0,10))
New-Item -ItemType Directory -Path $session -Force | Out-Null
$leaf=Split-Path -Leaf $session
$arguments="$($active.Id) config/layout.json recordings/$leaf"
$recorder=Start-Process -FilePath (Join-Path $PSScriptRoot 'build\recorder.exe') -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru -RedirectStandardError (Join-Path $session 'error.txt')
@{pid=$recorder.Id;directory=$session}|ConvertTo-Json|Set-Content 'recordings\active.json'
$deadline=[datetime]::UtcNow.AddSeconds(60)
while(!(Test-Path (Join-Path $session 'status.json'))){
 Start-Sleep -Milliseconds 200
 $recorder.Refresh()
 if($recorder.HasExited){throw (Get-Content (Join-Path $session 'error.txt') -Raw)}
 if([datetime]::UtcNow -gt $deadline){throw 'Map capture still running; inspect the session directory before starting another recorder.'}
}
Get-Content (Join-Path $session 'status.json')
Write-Output "Output: $session"
