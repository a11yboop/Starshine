param([switch]$CheckOnly,[double]$PreviewScale=1)
$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
Add-Type -Path (Join-Path $PSScriptRoot 'ui\Starshine.cs') -ReferencedAssemblies System.Windows.Forms,System.Drawing
$null=[Starshine.ShineForm]::SetProcessDPIAware()
[System.Windows.Forms.Application]::EnableVisualStyles()
Set-Location -LiteralPath $PSScriptRoot
$script:worker=$null
$script:logPath=$null
$script:errorPath=$null
$script:sessions=@()
$script:closing=$false
New-Item -ItemType Directory -Force -Path (Join-Path $PSScriptRoot 'control'),(Join-Path $PSScriptRoot 'recordings'),(Join-Path $PSScriptRoot 'logs') | Out-Null
$form=New-Object Starshine.ShineForm
$form.Font=New-Object Drawing.Font('Segoe UI',10)
function Label($text,$x,$y,$width,$height){$c=New-Object System.Windows.Forms.Label;$c.Text=$text;$c.BackColor=[Drawing.Color]::Transparent;$c.ForeColor=[Starshine.Art]::Muted;$c.AutoEllipsis=$true;$c.SetBounds($x,$y,$width,$height);$form.Controls.Add($c);return $c}
function Button($text,$x,$y,$width,$action){$c=New-Object Starshine.ShineButton;$c.Text=$text;$c.SetBounds($x,$y,$width,40);$c.FlatStyle='Flat';$c.BackColor=[System.Drawing.Color]::Transparent;$c.Add_Click($action);$form.Controls.Add($c);return $c}
$title=Label 'ANIME MACRO' 24 20 550 36
$title.Font=New-Object System.Drawing.Font('Segoe UI',21,[System.Drawing.FontStyle]::Bold)
$subtitle=Label 'Первая версия · запись игры и повтор действий по волнам' 26 62 800 28
$status=New-Object Starshine.StatusBadge;$form.Controls.Add($status)
$status.ForeColor=[System.Drawing.Color]::LightSkyBlue
$savedLabel=Label 'Saved record' 26 143 400 25
$list=New-Object Starshine.RecordList
$list.SetBounds(26,174,620,32);$list.DropDownStyle='DropDownList';$form.Controls.Add($list)
$details=New-Object Starshine.RecordStats;$form.Controls.Add($details)
$hint=Label 'Перед запуском открой нужную карту или экран Replay. После запуска верни фокус в Roblox. F8 — остановить.' 26 278 800 48
$limitations=Label 'v0.1: постановки, прокачка через R, волны и ожидание денег. Продажи, способности, вход из лобби и обход препятствий ещё не готовы.' 26 379 800 52
$limitations.ForeColor=[System.Drawing.Color]::Silver
$log=New-Object System.Windows.Forms.TextBox
$log.SetBounds(26,442,810,158);$log.Multiline=$true;$log.ReadOnly=$true;$log.ScrollBars='Vertical';$log.BackColor=[System.Drawing.Color]::FromArgb(15,18,24);$log.ForeColor=[System.Drawing.Color]::Gainsboro;$log.Font=New-Object System.Drawing.Font('Consolas',9);$log.Anchor='Top,Bottom,Left,Right';$form.Controls.Add($log)
function Refresh-Sessions {
 $previous=$list.SelectedItem;$list.Items.Clear();$script:sessions=@()
 foreach($folder in (Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'recordings') -Directory | Sort-Object LastWriteTime -Descending)){
  if(Test-Path (Join-Path $folder.FullName 'recording.json')){$script:sessions+= $folder;$null=$list.Items.Add(($folder.LastWriteTime.ToString('dd.MM HH:mm')+' · '+$folder.Name))}
 }
 if($previous -and $list.Items.Contains($previous)){$list.SelectedItem=$previous}elseif($list.Items.Count){$list.SelectedIndex=0}
}
$list.Add_SelectedIndexChanged({
 try{if($list.SelectedIndex -lt 0){return};$r=Get-Content (Join-Path $script:sessions[$list.SelectedIndex].FullName 'recording.json') -Raw | ConvertFrom-Json
 $placements=@($r.events | Where-Object {$_.type -in @('unit_placed','unit_discovered_after_gap')}).Count
 $upgrades=@($r.events | Where-Object {$_.type -eq 'unit_upgrade_changed'}).Count
 $details.Text="Placements: $placements     |     Upgrades: $upgrades"
 }catch{$details.Text='Не удалось прочитать запись: '+$_.Exception.Message}
})
function Start-Macro([bool]$record){
 try{
  if($script:worker -and !$script:worker.HasExited){throw 'Макрос уже запущен.'}
  if(@(Get-Process -Name recorder -ErrorAction SilentlyContinue).Count){throw 'Другой процесс макроса уже работает. Сначала останови его через F8.'}
  $clients=@(Get-Process -Name RobloxPlayerBeta -ErrorAction SilentlyContinue);if($clients.Count -ne 1){throw 'Открой один клиент Roblox.'}
  $exe=Join-Path $PSScriptRoot 'build\recorder.exe';if(!(Test-Path $exe)){throw 'Не найден build\recorder.exe. Выполни сборку.'}
  if(!$record -and $list.SelectedIndex -lt 0){throw 'Сначала выбери запись.'}
  $stop=Join-Path $PSScriptRoot 'control\STOP';if(Test-Path $stop){Remove-Item -LiteralPath $stop}
  $id=[guid]::NewGuid().ToString('N').Substring(0,10)
  $script:logPath=Join-Path $PSScriptRoot "logs\$id-output.txt";$script:errorPath=Join-Path $PSScriptRoot "logs\$id-error.txt"
  if($record){$folder="recordings/session-$id";New-Item -ItemType Directory -Path (Join-Path $PSScriptRoot $folder)|Out-Null;$arguments=@($clients[0].Id,'config/layout.json',$folder)}
  else{$folder='recordings/'+$script:sessions[$list.SelectedIndex].Name;$arguments=@('--arm-strategy',$clients[0].Id,'config/layout.json',('"'+$folder+'"'))}
  $script:isRecording=$record;$script:sessionFolder=Join-Path $PSScriptRoot $folder;$script:startedAt=[datetime]::UtcNow
  $script:worker=Start-Process -FilePath $exe -ArgumentList $arguments -WorkingDirectory $PSScriptRoot -WindowStyle Hidden -PassThru -RedirectStandardOutput $script:logPath -RedirectStandardError $script:errorPath
  $status.Text=if($record){'Recording'}else{'Running'}
  $recordButton.Enabled=$false;$playButton.Enabled=$false;$list.Enabled=$false;$stopButton.Enabled=$true;$log.Text=''
 }catch{$status.Text='Error · could not start';[System.Windows.Forms.MessageBox]::Show($_.Exception.Message,'Starshine')|Out-Null}
}
function Stop-Macro {if($script:worker -and !$script:worker.HasExited){New-Item -ItemType File -Force -Path (Join-Path $PSScriptRoot 'control\STOP')|Out-Null;$status.Text='Stopping · saving…'}}
$refreshButton=Button 'Обновить' 665 171 171 {Refresh-Sessions}
$recordButton=Button '● Новая запись' 26 331 210 {Start-Macro $true}
$playButton=Button '▶ Запустить макрос' 252 331 330 {Start-Macro $false}
$playButton.BackColor=[System.Drawing.Color]::FromArgb(26,112,91)
$stopButton=Button '■ Остановить' 598 331 238 {Stop-Macro};$stopButton.Enabled=$false
$openButton=Button 'Папка записей' 26 613 190 {Start-Process explorer.exe -ArgumentList ('"'+(Join-Path $PSScriptRoot 'recordings')+'"')}
$priceButton=Button 'Цены прокачки' 230 613 190 {Start-Process notepad.exe -ArgumentList ('"'+(Join-Path $PSScriptRoot 'config\upgrade-costs.json')+'"')}
$null=Label 'Цена первого улучшения Bulby: 1500. Для других юнитов заполни таблицу цен.' 435 610 400 45
$timer=New-Object System.Windows.Forms.Timer;$timer.Interval=400
$timer.Add_Tick({
 if(!$script:worker){$hud.Hide();return}
 Update-Hud
 try{
  $text='';if(Test-Path $script:logPath){$text=Get-Content $script:logPath -Raw};if(Test-Path $script:errorPath){$errors=Get-Content $script:errorPath -Raw;if($errors){$text+="`r`n"+$errors}}
  if($log.Text -ne $text){$log.Text=$text;$log.SelectionStart=$log.Text.Length;$log.ScrollToCaret()}
  $script:worker.Refresh();if($script:worker.HasExited){$code=$script:worker.ExitCode;$script:worker.Dispose();$script:worker=$null;$hud.Hide();$recordButton.Enabled=$true;$playButton.Enabled=$true;$list.Enabled=$true;$stopButton.Enabled=$false;$status.Text=if($code -eq 0){'Stopped · saved'}else{'Error · see Details'};Refresh-Sessions;if($script:closing){$form.Close()}}
 }catch{$status.Text='Error · reading log'}
})
$form.Add_FormClosing({param($sender,$e)if($script:worker -and !$script:worker.HasExited){$e.Cancel=$true;$script:closing=$true;Stop-Macro}})
. (Join-Path $PSScriptRoot 'ui\ApplyTheme.ps1')
Add-Type -ReferencedAssemblies System.Windows.Forms,System.Drawing -TypeDefinition @"
using System;
using System.Drawing;
using System.Windows.Forms;
using System.Runtime.InteropServices;
public sealed class MacroHud : Form {
 public string StatusText="";
 public string DetailText="F8 · остановить";
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [StructLayout(LayoutKind.Sequential)] public struct RECT {public int L,T,R,B;}
 public MacroHud(){FormBorderStyle=FormBorderStyle.None;ShowInTaskbar=false;TopMost=true;Size=new Size(250,65);BackColor=Color.FromArgb(24,29,38);DoubleBuffered=true;}
 protected override bool ShowWithoutActivation {get{return true;}}
 protected override CreateParams CreateParams {get{var p=base.CreateParams;p.ExStyle|=0x08000000|0x20|0x80;return p;}}
 protected override void OnPaint(PaintEventArgs e){base.OnPaint(e);e.Graphics.SmoothingMode=System.Drawing.Drawing2D.SmoothingMode.AntiAlias;using(var b=new SolidBrush(Color.FromArgb(244,91,111)))e.Graphics.FillEllipse(b,16,17,9,9);using(var f=new Font("Segoe UI",11,FontStyle.Bold))e.Graphics.DrawString(StatusText,f,Brushes.White,34,10);using(var f=new Font("Segoe UI",9))e.Graphics.DrawString(DetailText,f,Brushes.LightGray,34,37);}
}
"@
$hud=New-Object MacroHud
function Update-Hud {
 try{
  $clients=@(Get-Process -Name RobloxPlayerBeta -ErrorAction SilentlyContinue)
  if($clients.Count -ne 1 -or [MacroHud]::GetForegroundWindow() -ne $clients[0].MainWindowHandle){$hud.Hide();return}
  $rect=New-Object MacroHud+RECT
  if(![MacroHud]::GetWindowRect($clients[0].MainWindowHandle,[ref]$rect)){return}
  $hud.Location=New-Object Drawing.Point(($rect.R-270),($rect.T+100))
  $elapsed=[datetime]::UtcNow-$script:startedAt
  $hud.StatusText=if($script:isRecording){'Запись · '+$elapsed.ToString('mm\:ss')}else{'Макрос работает'}
  $hud.DetailText=if($script:isRecording){'F8 · остановить и сохранить'}else{'F8 · остановить'}
  if($script:isRecording){$file=Join-Path $script:sessionFolder 'status.json';if(!(Test-Path $file)){$hud.StatusText='Подготовка записи…'}else{try{$st=Get-Content $file -Raw|ConvertFrom-Json;if($st.state -eq 'waiting_for_valid_data'){$hud.StatusText='Ожидание данных…'}}catch{}}}
  if(!$hud.Visible){$hud.Show()};$hud.Invalidate()
 }catch{$hud.Hide()}
}
$form.Add_FormClosed({$hud.Dispose()})
Refresh-Sessions
if($CheckOnly){. (Join-Path $PSScriptRoot 'ui\CheckTheme.ps1');exit 0}
$timer.Start();[System.Windows.Forms.Application]::Run($form);$timer.Dispose()
