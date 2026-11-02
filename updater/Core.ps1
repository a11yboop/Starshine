$ErrorActionPreference='Stop'
function Resolve-PackagePath([string]$Root,[string]$Relative) {
 if(!$Relative -or $Relative.Contains('\') -or $Relative -match '(^/|:|(^|/)\.\.?(/|$))'){throw 'Unsafe package path'}
 $base=[IO.Path]::GetFullPath($Root).TrimEnd('\')+'\'
 $path=[IO.Path]::GetFullPath((Join-Path $Root $Relative))
 if(!$path.StartsWith($base,[StringComparison]::OrdinalIgnoreCase)){throw 'Package path escapes root'}
 return $path
}
function Test-ManagedPath([string]$Path) {
 return ($Path -match '^(Launcher\.ps1|Starshine\.exe|AnimeMacro\.exe|app-version\.json|README\.md|THIRD-PARTY-NOTICES\.txt|build/recorder\.exe|ui/[A-Za-z0-9_.-]+\.(ps1|cs)|updater/[A-Za-z0-9_.-]+\.ps1|config/(layout|unit-names|lobby-maps|upgrade-costs)\.json)$')
}
function Test-UpdatePackage([string]$Stage) {
 $manifest=Get-Content -LiteralPath (Join-Path $Stage 'release-manifest.json') -Raw | ConvertFrom-Json
 if($manifest.schema -ne 1 -or $manifest.version -notmatch '^\d+\.\d+\.\d+$'){throw 'Unsupported release manifest'}
 $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase)
 foreach($entry in $manifest.files){
  if(!(Test-ManagedPath $entry.path) -or !$seen.Add($entry.path)){throw 'Unexpected or duplicate release file'}
  $file=Resolve-PackagePath $Stage $entry.path
  if($entry.sha256 -notmatch '^[a-fA-F0-9]{64}$' -or !(Test-Path -LiteralPath $file -PathType Leaf) -or (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $entry.sha256){throw ('Package integrity failed: '+$entry.path)}
 }
 foreach($required in @('Launcher.ps1','Starshine.exe','build/recorder.exe','app-version.json','updater/Core.ps1','updater/Apply.ps1','ui/Updates.ps1')){if(!$seen.Contains($required)){throw ('Incomplete package: '+$required)}}
 $version=Get-Content -LiteralPath (Join-Path $Stage 'app-version.json') -Raw|ConvertFrom-Json
 if($version.version -ne $manifest.version -or $version.repository -ne 'a11yboop/Starshine'){throw 'Release identity mismatch'}
 return $manifest
}
function Expand-UpdatePackage([string]$Zip,[string]$Stage,[string]$ExpectedHash) {
 if($ExpectedHash -notmatch '^[a-fA-F0-9]{64}$' -or (Get-FileHash -LiteralPath $Zip -Algorithm SHA256).Hash -ne $ExpectedHash){throw 'Download checksum mismatch'}
 Add-Type -AssemblyName System.IO.Compression,System.IO.Compression.FileSystem
 $archive=[IO.Compression.ZipFile]::OpenRead($Zip)
 try{
  $seen=New-Object 'Collections.Generic.HashSet[string]' ([StringComparer]::OrdinalIgnoreCase);[long]$total=0
  foreach($entry in $archive.Entries){
   $null=Resolve-PackagePath $Stage $entry.FullName
   if(!$seen.Add($entry.FullName) -or ($entry.FullName -ne 'release-manifest.json' -and !(Test-ManagedPath $entry.FullName))){throw 'Unexpected ZIP entry'}
   $total+=$entry.Length;if($total -gt 300MB){throw 'Package is too large'}
  }
 }finally{$archive.Dispose()}
 [IO.Compression.ZipFile]::ExtractToDirectory($Zip,$Stage)
 return Test-UpdatePackage $Stage
}
function Install-UpdatePackage([string]$Stage,[string]$Install,[string]$Backup) {
 $manifest=Test-UpdatePackage $Stage
 $null=New-Item -ItemType Directory -Path $Backup -Force
 $changed=New-Object Collections.Generic.List[object]
 try{
  foreach($entry in $manifest.files){
   $destination=Resolve-PackagePath $Install $entry.path
   # User-edited prices are mutable data, even when shipping defaults.
   if($entry.path -eq 'config/upgrade-costs.json' -and (Test-Path -LiteralPath $destination)){continue}
   $exists=Test-Path -LiteralPath $destination -PathType Leaf
   $old=Resolve-PackagePath $Backup $entry.path
   if($exists){$null=New-Item -ItemType Directory -Force -Path (Split-Path $old);Copy-Item -LiteralPath $destination -Destination $old}
   $null=New-Item -ItemType Directory -Force -Path (Split-Path $destination)
   $changed.Add([pscustomobject]@{path=$entry.path;existed=$exists})
   Copy-Item -LiteralPath (Resolve-PackagePath $Stage $entry.path) -Destination $destination -Force
  }
  $changed | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $Backup 'rollback-files.json') -Encoding UTF8
 }catch{
  $failure=$_
  for($i=$changed.Count-1;$i -ge 0;$i--){$item=$changed[$i];$destination=Resolve-PackagePath $Install $item.path
   if($item.existed){Copy-Item -LiteralPath (Resolve-PackagePath $Backup $item.path) -Destination $destination -Force}
   elseif(Test-Path -LiteralPath $destination -PathType Leaf){Remove-Item -LiteralPath $destination}
  }
  throw $failure
 }
}
function Get-StarshineReleases {
 [Net.ServicePointManager]::SecurityProtocol=[Net.SecurityProtocolType]::Tls12
 $all=@()
 for($page=1;$page -le 10;$page++){
  $response=Invoke-RestMethod -Uri ('https://api.github.com/repos/a11yboop/Starshine/releases?per_page=100&page='+$page) -Headers @{'User-Agent'='Starshine-Updater';'Accept'='application/vnd.github+json'} -TimeoutSec 20
  $batch=@($response)
  $all+=@($batch|Where-Object {!$_.draft -and !$_.prerelease -and $_.tag_name -match '^v\d+\.\d+\.\d+$' -and @($_.assets|Where-Object name -eq 'Starshine-win-x64.zip').Count -eq 1 -and @($_.assets|Where-Object name -eq 'Starshine-win-x64.zip.sha256').Count -eq 1})
  if($batch.Count -lt 100){break}
 }
 return @($all|Sort-Object {[version]$_.tag_name.TrimStart('v')} -Descending)
}
