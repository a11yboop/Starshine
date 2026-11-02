function Show-LobbySettings {
 if($script:worker -and !$script:worker.HasExited){return}
 if($list.SelectedIndex -lt 0){[Windows.Forms.MessageBox]::Show('Choose a saved record first.','Starshine')|Out-Null;return}
 $file=Join-Path $script:sessions[$list.SelectedIndex].FullName 'lobby.json'
 $maps=@(Get-Content (Join-Path $PSScriptRoot '..\config\lobby-maps.json') -Raw|ConvertFrom-Json)
 $dialog=New-Object Windows.Forms.Form;$dialog.Text='Starshine · Lobby setup';$dialog.ClientSize=New-Object Drawing.Size(450,310);$dialog.StartPosition='CenterParent';$dialog.FormBorderStyle='FixedDialog';$dialog.MaximizeBox=$false;$dialog.MinimizeBox=$false;$dialog.BackColor=[Drawing.ColorTranslator]::FromHtml('#261B39');$dialog.ForeColor=[Starshine.Art]::Text;$dialog.Font=New-Object Drawing.Font('Segoe UI',10)
 $enabled=New-Object Windows.Forms.CheckBox;$enabled.Text='Enter this level from the lobby before Run';$enabled.SetBounds(24,20,405,30);$dialog.Controls.Add($enabled)
 function Field($text,$y,$items){$label=New-Object Windows.Forms.Label;$label.Text=$text;$label.SetBounds(24,$y,110,27);$dialog.Controls.Add($label);$combo=New-Object Windows.Forms.ComboBox;$combo.DropDownStyle='DropDownList';$combo.SetBounds(140,$y,280,30);foreach($item in $items){$null=$combo.Items.Add($item)};$combo.SelectedIndex=0;$dialog.Controls.Add($combo);return $combo}
 $map=Field 'Map' 70 @($maps|ForEach-Object {$_.name})
 $act=Field 'Mode / act' 115 @('Act 1','Act 2','Act 3','Act 4','Act 5','Act 6','Infinite')
 $difficulty=Field 'Difficulty' 160 @('Normal','Hard')
 $act.Add_SelectedIndexChanged({if($act.SelectedIndex -eq 6){$difficulty.SelectedIndex=1;$difficulty.Enabled=$false}else{$difficulty.Enabled=$true}})
 if(Test-Path $file){try{$profile=Get-Content $file -Raw|ConvertFrom-Json;$enabled.Checked=[bool]$profile.enabled;for($i=0;$i -lt $maps.Count;$i++){if($maps[$i].id -eq $profile.world){$map.SelectedIndex=$i}};$act.SelectedIndex=if($profile.act -eq 0){6}else{[int]$profile.act-1};$difficulty.SelectedItem=$profile.difficulty}catch{}}
 $note=New-Object Windows.Forms.Label;$note.Text='Saved for this recording. Event and Legend routes are not configured.';$note.SetBounds(24,205,395,44);$note.ForeColor=[Starshine.Art]::Muted;$dialog.Controls.Add($note)
 $save=New-Object Windows.Forms.Button;$save.Text='Save';$save.SetBounds(310,260,110,32);$save.DialogResult='OK';$dialog.Controls.Add($save);$dialog.AcceptButton=$save
 $cancel=New-Object Windows.Forms.Button;$cancel.Text='Cancel';$cancel.SetBounds(190,260,110,32);$cancel.DialogResult='Cancel';$dialog.Controls.Add($cancel);$dialog.CancelButton=$cancel
 try{if($dialog.ShowDialog($form) -eq 'OK'){
  $profile=@{enabled=$enabled.Checked;world=$maps[$map.SelectedIndex].id;act=$(if($act.SelectedIndex -eq 6){0}else{$act.SelectedIndex+1});difficulty=$difficulty.SelectedItem}
  [IO.File]::WriteAllText($file,($profile|ConvertTo-Json));$status.Text='Lobby settings saved'
 }}finally{$dialog.Dispose()}
}
