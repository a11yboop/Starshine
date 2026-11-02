param([string]$Version='0.5.1')
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot
. (Join-Path $root 'updater\Core.ps1')
$release=Join-Path $root ('dist\release-'+$Version+'-'+[guid]::NewGuid().ToString('N'))
$stage=Join-Path $release 'stage';$null=New-Item -ItemType Directory -Path $stage -Force
$files=@('Launcher.ps1','Starshine.exe','AnimeMacro.exe','app-version.json','README.md','THIRD-PARTY-NOTICES.txt','build/recorder.exe','config/layout.json','config/unit-names.json','config/lobby-maps.json','config/upgrade-costs.json')
foreach($dir in @('ui','updater')){$files+=@(Get-ChildItem -LiteralPath (Join-Path $root $dir) -File|Where-Object {$_.Extension -in @('.ps1','.cs')}|ForEach-Object {$dir+'/'+$_.Name})}
$entries=@()
foreach($relative in $files){
 $destination=Resolve-PackagePath $stage $relative;$null=New-Item -ItemType Directory -Path (Split-Path $destination) -Force
 Copy-Item -LiteralPath (Join-Path $root $relative) -Destination $destination
 $entries+=@{path=$relative;sha256=(Get-FileHash -LiteralPath $destination -Algorithm SHA256).Hash.ToLowerInvariant()}
}
@{schema=1;version=$Version;files=$entries}|ConvertTo-Json -Depth 5|Set-Content -LiteralPath (Join-Path $stage 'release-manifest.json') -Encoding UTF8
$null=Test-UpdatePackage $stage
Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
$zip=Join-Path $release 'Starshine-win-x64.zip'
# Create portable forward-slash names rather than platform-dependent ZIP paths.
$archive=[IO.Compression.ZipFile]::Open($zip,[IO.Compression.ZipArchiveMode]::Create)
try{foreach($relative in ($files+@('release-manifest.json'))){$null=[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive,(Join-Path $stage $relative),$relative)}}finally{$archive.Dispose()}
((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant()+'  Starshine-win-x64.zip')|Set-Content -LiteralPath ($zip+'.sha256') -Encoding ASCII
Write-Output $release
