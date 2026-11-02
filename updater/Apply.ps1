param([Parameter(Mandatory=$true)][string]$Plan)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
try {
 . (Join-Path $PSScriptRoot 'Core.ps1')
 $p=Get-Content -LiteralPath $Plan -Raw|ConvertFrom-Json
 $process=Get-Process -Id $p.parentPid -ErrorAction SilentlyContinue
 if($process -and !$process.WaitForExit(30000)){throw 'Close Starshine before installing the update.'}
 if(Get-Process recorder -ErrorAction SilentlyContinue){throw 'Stop the macro before updating.'}
 Install-UpdatePackage $p.stage $p.install $p.backup
 Start-Process -FilePath (Join-Path $p.install 'Starshine.exe') -WorkingDirectory $p.install -WindowStyle Hidden
} catch {
 $_.Exception.ToString()|Set-Content -LiteralPath (Join-Path $PSScriptRoot 'update-error.txt')
 [Windows.Forms.MessageBox]::Show(('Update failed. '+$_.Exception.Message),'Starshine update')|Out-Null
 exit 1
}
