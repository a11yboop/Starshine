# Presentation only: macro handlers remain in Launcher.ps1.
foreach($c in $form.Controls){$c.Visible=$false}
function Place($control,$x,$y,$w,$h){$control.Visible=$true;$form.Register($control,$x,$y,$w,$h)}
function StyleButton($control,$text,$symbol,$primary=$false){$control.Text=$text;$control.Symbol=$symbol;$control.Primary=$primary;$control.Font=New-Object Drawing.Font('Segoe UI',11);$control.AccessibleName=$text}
$title.Text='Starshine';$title.Font=New-Object Drawing.Font('Segoe UI',25,([Drawing.FontStyle]::Bold -bor [Drawing.FontStyle]::Italic));$title.ForeColor=[Drawing.ColorTranslator]::FromHtml('#F0C9ED')
$title.Add_MouseDown({param($sender,$e)if($e.Button -eq 'Left'){$form.Drag()}})
Place $title 82 14 320 51
$status.Text='Ready to work';$status.Font=New-Object Drawing.Font('Segoe UI',10,[Drawing.FontStyle]::Bold)
Place $status 45 106 235 34
$savedLabel.Text='Saved record';$savedLabel.Font=New-Object Drawing.Font('Segoe UI',13,[Drawing.FontStyle]::Bold)
Place $savedLabel 77 155 700 28
$list.Font=New-Object Drawing.Font('Segoe UI',11);$list.DropDownWidth=710;$list.AccessibleName='Saved record';$list.TabIndex=0
Place $list 46 191 627 39
StyleButton $refreshButton 'Refresh' 'refresh';$refreshButton.TabIndex=1
Place $refreshButton 690 190 164 39
$details.Font=New-Object Drawing.Font('Segoe UI',10);$details.ForeColor=[Drawing.ColorTranslator]::FromHtml('#E5A1CF')
$details.Text='Select a record, or create a new one.'
Place $details 48 244 805 26
$positionHint=Label 'Unit positions on the map · actions by wave number.' 0 0 1 1
$positionHint.Font=New-Object Drawing.Font('Segoe UI',9)
Place $positionHint 76 280 777 22
$hint.Text='Open the map or Replay. After starting, switch to Roblox.';$hint.Font=New-Object Drawing.Font('Segoe UI',9)
Place $hint 76 307 777 23
StyleButton $recordButton 'Record' 'record';$recordButton.TabIndex=2
StyleButton $playButton 'Run' 'run' $true;$playButton.Font=New-Object Drawing.Font('Segoe UI',13,[Drawing.FontStyle]::Bold);$playButton.TabIndex=3
StyleButton $stopButton 'Stop · F8' 'stop';$stopButton.TabIndex=4
Place $recordButton 46 345 232 49
Place $playButton 294 345 312 49
Place $stopButton 622 345 232 49
StyleButton $openButton 'My Records' 'folder';$openButton.TabIndex=5
StyleButton $priceButton 'Upgrade Prices' 'prices';$priceButton.TabIndex=6
$moreButton=Button 'Details' 0 0 1 {$log.Visible=!$log.Visible;$lobbyButton.Visible=$log.Visible;$form.SetDetails($log.Visible);$moreButton.Text=if($log.Visible){'Hide details'}else{'Details'}}
StyleButton $moreButton 'Details' 'info';$moreButton.TabIndex=7
Place $openButton 46 410 258 36
Place $priceButton 321 410 258 36
Place $moreButton 596 410 258 36
$log.BackColor=[Drawing.ColorTranslator]::FromHtml('#261B39');$log.ForeColor=[Starshine.Art]::Muted;$log.BorderStyle='FixedSingle';$log.Anchor='Top,Left'
$form.Register($log,46,516,808,96);$log.Visible=$false;$log.TabIndex=8
$min=Button '' 0 0 1 {$form.WindowState='Minimized'}
$max=Button '' 0 0 1 {$form.WindowState=if($form.WindowState -eq 'Maximized'){'Normal'}else{'Maximized'}}
$close=Button '' 0 0 1 {$form.Close()}
foreach($entry in @(@($min,'min','Minimize',746),@($max,'max','Maximize or restore',792),@($close,'close','Close',838))){$c=$entry[0];$c.Chrome=$true;$c.Symbol=$entry[1];$c.AccessibleName=$entry[2];$c.Font=New-Object Drawing.Font('Segoe UI',11);Place $c $entry[3] 18 36 36}
$form.Text='Starshine';$form.ForeColor=[Starshine.Art]::Text
$form.ClientSize=New-Object Drawing.Size(900,470)
$graphics=$form.CreateGraphics();$dpiScale=$graphics.DpiX/96;$graphics.Dispose()
if($CheckOnly){$dpiScale=$PreviewScale}
$form.ClientSize=New-Object Drawing.Size([int](900*$dpiScale),[int](470*$dpiScale))
$form.Arrange()

. (Join-Path $PSScriptRoot 'LobbySettings.ps1')
$lobbyButton=Button 'Lobby setup' 0 0 1 {Show-LobbySettings}
StyleButton $lobbyButton 'Lobby setup' 'prices'
$form.Register($lobbyButton,46,475,210,30);$lobbyButton.Visible=$false

. (Join-Path $PSScriptRoot "Updates.ps1")
