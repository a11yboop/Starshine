param([string]$Session,[switch]$ValidateOnly,[switch]$WaitForMatch,[switch]$Strategy)
$ErrorActionPreference='Stop'
Set-Location -LiteralPath $PSScriptRoot
if(!$Session){
 Add-Type -AssemblyName System.Windows.Forms
 $dialog=New-Object System.Windows.Forms.OpenFileDialog
 $dialog.Title='Choose a recording'
 $dialog.Filter='Recorded configuration (recording.json)|recording.json'
 $dialog.InitialDirectory=Join-Path $PSScriptRoot 'recordings'
 if($dialog.ShowDialog() -ne 'OK'){exit}
 $Session=Split-Path -Parent $dialog.FileName
}
$active=@(Get-Process -Name RobloxPlayerBeta -ErrorAction SilentlyContinue)
if($active.Count -ne 1){throw 'Exactly one Roblox client is required.'}
$mode=if($ValidateOnly){'--validate-replay'}elseif($Strategy){'--arm-strategy'}elseif($WaitForMatch){'--arm-replay'}else{'--replay'}
& '.\build\recorder.exe' $mode $active[0].Id 'config\layout.json' $Session
if($LASTEXITCODE -ne 0){throw 'Replay stopped or preflight failed; see the message above.'}

