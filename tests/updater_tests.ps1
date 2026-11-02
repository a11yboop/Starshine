param([Parameter(Mandatory=$true)][string]$Package)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
. (Join-Path $root 'updater\Core.ps1')
$test=Join-Path $root ('build\update-test-'+[guid]::NewGuid().ToString('N'));$null=New-Item -ItemType Directory -Path $test
$zip=Join-Path $Package 'Starshine-win-x64.zip';$hash=(Get-FileHash $zip).Hash
$stage=Join-Path $test 'stage';$manifest=Expand-UpdatePackage $zip $stage $hash
$install=Join-Path $test 'install';$null=New-Item -ItemType Directory -Force -Path (Join-Path $install 'recordings'),(Join-Path $install 'config')
'personal-record'|Set-Content (Join-Path $install 'recordings\mine.json')
'custom-prices'|Set-Content (Join-Path $install 'config\upgrade-costs.json')
'old-launcher'|Set-Content (Join-Path $install 'Launcher.ps1')
Install-UpdatePackage $stage $install (Join-Path $test 'backup')
if((Get-Content (Join-Path $install 'recordings\mine.json')) -ne 'personal-record'){throw 'Recording overwritten'}
if((Get-Content (Join-Path $install 'config\upgrade-costs.json')) -ne 'custom-prices'){throw 'Prices overwritten'}
if((Get-Content (Join-Path $test 'backup\Launcher.ps1')) -ne 'old-launcher'){throw 'Backup missing'}
# A previous package version is accepted; local data remains untouched on rollback.
$versionFile=Join-Path $install 'app-version.json';'{"version":"9.0.0"}'|Set-Content $versionFile
Install-UpdatePackage $stage $install (Join-Path $test 'downgrade-backup')
if((Get-Content $versionFile -Raw|ConvertFrom-Json).version -ne $manifest.version){throw 'Downgrade failed'}
$rejected=$false;try{Expand-UpdatePackage $zip (Join-Path $test 'bad-hash') ('0'*64)}catch{$rejected=$true};if(!$rejected){throw 'Bad checksum accepted'}
foreach($path in @('../escape','C:/escape','ui/../../escape','ui\escape')){$rejected=$false;try{Resolve-PackagePath $stage $path}catch{$rejected=$true};if(!$rejected){throw 'Unsafe path accepted'}}
# Force an I/O failure after earlier files have been replaced; all changes roll back.
$broken=Join-Path $test 'broken';$null=New-Item -ItemType Directory -Path $broken
'original'|Set-Content (Join-Path $broken 'Launcher.ps1');'blocks-build-directory'|Set-Content (Join-Path $broken 'build')
$rejected=$false;try{Install-UpdatePackage $stage $broken (Join-Path $test 'failed-backup')}catch{$rejected=$true}
if(!$rejected -or (Get-Content (Join-Path $broken 'Launcher.ps1')) -ne 'original' -or (Test-Path (Join-Path $broken 'Starshine.exe'))){throw 'Rollback failed'}
Add-Content -LiteralPath (Join-Path $stage 'Launcher.ps1') -Value 'tampered'
$rejected=$false;try{Test-UpdatePackage $stage}catch{$rejected=$true};if(!$rejected){throw 'Tampered file accepted'}
Write-Output 'PASS: update, downgrade, data preservation, backup, rollback, checksum, unsafe path and tamper rejection'
