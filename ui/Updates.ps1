. (Join-Path $PSScriptRoot '..\updater\Core.ps1')
$script:installRoot=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$script:appVersion=(Get-Content -LiteralPath (Join-Path $script:installRoot 'app-version.json') -Raw|ConvertFrom-Json).version
function Show-UpdateVersions {
 $dialog=New-Object Windows.Forms.Form
 $dialog.Text='Starshine - Versions';$dialog.Size=New-Object Drawing.Size(550,345);$dialog.StartPosition='CenterParent';$dialog.FormBorderStyle='FixedDialog';$dialog.MaximizeBox=$false;$dialog.MinimizeBox=$false
 $dialog.BackColor=[Drawing.ColorTranslator]::FromHtml('#30223F');$dialog.ForeColor=[Drawing.Color]::White;$dialog.Font=New-Object Drawing.Font('Segoe UI',10)
 $current=New-Object Windows.Forms.Label;$current.SetBounds(20,20,490,28);$current.Text='Installed: '+$script:appVersion;$dialog.Controls.Add($current)
 $versions=New-Object Windows.Forms.ComboBox;$versions.SetBounds(20,60,490,30);$versions.DropDownStyle='DropDownList';$dialog.Controls.Add($versions)
 $notes=New-Object Windows.Forms.TextBox;$notes.SetBounds(20,104,490,115);$notes.Multiline=$true;$notes.ReadOnly=$true;$notes.ScrollBars='Vertical';$dialog.Controls.Add($notes)
 $install=New-Object Windows.Forms.Button;$install.SetBounds(20,239,235,40);$install.Text='Install selected version';$install.Enabled=$false;$dialog.Controls.Add($install)
 $refresh=New-Object Windows.Forms.Button;$refresh.SetBounds(275,239,235,40);$refresh.Text='Refresh releases';$dialog.Controls.Add($refresh)
 $dialog.Tag=@{releases=@()}
 $versions.Add_SelectedIndexChanged({if($versions.SelectedIndex -ge 0){$release=$dialog.Tag.releases[$versions.SelectedIndex];$notes.Text=$release.body;$install.Enabled=$release.tag_name -ne ('v'+$script:appVersion)}})
 $load={
  try{$refresh.Enabled=$false;$dialog.UseWaitCursor=$true;$notes.Text='Checking GitHub releases...';[Windows.Forms.Application]::DoEvents()
   $dialog.Tag.releases=@(Get-StarshineReleases);$versions.Items.Clear();foreach($r in $dialog.Tag.releases){$null=$versions.Items.Add($r.tag_name)}
   if($versions.Items.Count){$versions.SelectedIndex=0}else{$notes.Text='No compatible releases published yet.'}
  }catch{$notes.Text='Could not check releases: '+$_.Exception.Message}finally{$refresh.Enabled=$true;$dialog.UseWaitCursor=$false}
 }
 $refresh.Add_Click($load);$dialog.Add_Shown($load)
 $install.Add_Click({
  try {
   if(Get-Process recorder -ErrorAction SilentlyContinue){throw 'Stop the macro before updating.'}
   $release=$dialog.Tag.releases[$versions.SelectedIndex];$install.Enabled=$false;$refresh.Enabled=$false;$versions.Enabled=$false;$dialog.UseWaitCursor=$true
   $notes.Text='Downloading and verifying '+$release.tag_name+'...';[Windows.Forms.Application]::DoEvents()
   $job=Join-Path $script:installRoot ('.updates\'+[guid]::NewGuid().ToString('N'));$null=New-Item -ItemType Directory -Path $job
   $zip=Join-Path $job 'package.zip';$stage=Join-Path $job 'stage'
   $asset=@($release.assets|Where-Object name -eq 'Starshine-win-x64.zip')[0];$checksum=@($release.assets|Where-Object name -eq 'Starshine-win-x64.zip.sha256')[0]
   $prefix='https://github.com/a11yboop/Starshine/releases/download/'+$release.tag_name+'/'
   foreach($a in @($asset,$checksum)){if(!$a.browser_download_url.StartsWith($prefix,[StringComparison]::Ordinal)){throw 'Unexpected release download URL'}}
   $hashText=(Invoke-WebRequest -UseBasicParsing -Uri $checksum.browser_download_url -TimeoutSec 30).Content
   $hash=($hashText.Trim() -split '\s+')[0]
   Invoke-WebRequest -UseBasicParsing -Uri $asset.browser_download_url -OutFile $zip -TimeoutSec 180
   $manifest=Expand-UpdatePackage $zip $stage $hash
   if(('v'+$manifest.version) -ne $release.tag_name){throw 'Selected version does not match package'}
   if(Get-Process recorder -ErrorAction SilentlyContinue){throw 'Macro started during download; installation cancelled.'}
   foreach($file in @('Core.ps1','Apply.ps1')){Copy-Item -LiteralPath (Join-Path $script:installRoot ('updater\'+$file)) -Destination (Join-Path $job $file)}
   $plan=Join-Path $job 'plan.json';@{install=$script:installRoot;stage=$stage;backup=(Join-Path $job 'backup');parentPid=$PID}|ConvertTo-Json|Set-Content -LiteralPath $plan -Encoding UTF8
   $null=Start-Process powershell.exe -ArgumentList @('-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+(Join-Path $job 'Apply.ps1')+'"'),'-Plan',('"'+$plan+'"')) -WindowStyle Hidden -PassThru
   $dialog.Close();$form.Close()
  }catch{$notes.Text='Update failed: '+$_.Exception.Message;$install.Enabled=$true;$refresh.Enabled=$true;$versions.Enabled=$true;$dialog.UseWaitCursor=$false}
 })
 $null=$dialog.ShowDialog($form);$dialog.Dispose()
}
$updateButton=Button ('Versions · '+$script:appVersion) 0 0 1 {Show-UpdateVersions}
StyleButton $updateButton ('Versions · '+$script:appVersion) 'refresh'
Place $updateButton 625 106 229 34
# Background check only; never interrupt recording or install without selection.
if(!$CheckOnly){
 $script:releaseJob=Start-Job -ArgumentList (Join-Path $script:installRoot 'updater\Core.ps1') -ScriptBlock {param($core) . $core; @(Get-StarshineReleases)|Select-Object -First 1 -ExpandProperty tag_name}
 $updateTimer=New-Object Windows.Forms.Timer;$updateTimer.Interval=1500
 $updateTimer.Add_Tick({if($script:releaseJob.State -in @('Completed','Failed','Stopped')){
  $tag=Receive-Job $script:releaseJob -ErrorAction SilentlyContinue
  if($tag -match '^v\d+\.\d+\.\d+$' -and [version]$tag.TrimStart('v') -gt [version]$script:appVersion){$updateButton.Text='Update available · '+$tag}
  Remove-Job $script:releaseJob -Force;$script:releaseJob=$null;$updateTimer.Stop()
 }})
 $updateTimer.Start()
 $form.Add_FormClosed({$updateTimer.Stop();$updateTimer.Dispose();if($script:releaseJob){Stop-Job $script:releaseJob;Remove-Job $script:releaseJob -Force}})
}
