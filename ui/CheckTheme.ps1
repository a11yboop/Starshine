$form.Show()
[Windows.Forms.Application]::DoEvents()
function Save-Preview($name){
 $bitmap=New-Object Drawing.Bitmap($form.Width,$form.Height)
 try{$form.DrawToBitmap($bitmap,(New-Object Drawing.Rectangle(0,0,$form.Width,$form.Height)));$bitmap.Save((Join-Path $PSScriptRoot ('..\build\'+$name+'.png')))}finally{$bitmap.Dispose()}
}
$count=$list.Items.Count;$selected=$list.SelectedItem
$refreshButton.PerformClick()
if($list.Items.Count -ne $count -or $list.SelectedItem -ne $selected){throw 'Refresh changed selected record'}
if($count -gt 0 -and $details.Text -notlike 'Placements:*'){throw 'Record summary did not update'}
if($stopButton.Enabled){throw 'Stop should be disabled while idle'}
foreach($c in @($title,$status,$list,$refreshButton,$details,$recordButton,$playButton,$stopButton,$openButton,$priceButton,$moreButton,$close)){
 if(!$form.ClientRectangle.Contains($c.Bounds)){throw ('Control out of bounds: '+$c.Text)}
}
Save-Preview ('starshine-preview-'+$PreviewScale)
$moreButton.PerformClick();[Windows.Forms.Application]::DoEvents()
if(!$log.Visible -or !$form.ClientRectangle.Contains($log.Bounds)){throw 'Details did not expand within window'}
Save-Preview ('starshine-details-'+$PreviewScale)
$moreButton.PerformClick();if($log.Visible){throw 'Details did not collapse'}
$max.PerformClick();[Windows.Forms.Application]::DoEvents()
if($form.WindowState -ne 'Maximized'){throw 'Maximize failed'}
$max.PerformClick();[Windows.Forms.Application]::DoEvents()
if($form.WindowState -ne 'Normal'){throw 'Restore failed'}
$min.PerformClick();if($form.WindowState -ne 'Minimized'){throw 'Minimize failed'}
$form.WindowState='Normal'
for($i=0;$i -lt 8;$i++){
 $form.WindowState='Minimized';[Windows.Forms.Application]::DoEvents();$form.Arrange()
 $form.WindowState='Normal';[Windows.Forms.Application]::DoEvents();$form.Arrange()
 foreach($c in $form.Controls){if($c.Font.Size -le 0){throw 'Invalid font after restoring window'}}
}
Save-Preview ('starshine-restored-'+$PreviewScale)
$enter=[Starshine.ShineButton].GetMethod('OnMouseEnter',[Reflection.BindingFlags]'Instance,NonPublic')
$leave=[Starshine.ShineButton].GetMethod('OnMouseLeave',[Reflection.BindingFlags]'Instance,NonPublic')
foreach($c in @($min,$max,$close,$refreshButton,$recordButton,$playButton,$moreButton)){
 $null=$enter.Invoke($c,@([EventArgs]::Empty));$c.Refresh()
 $null=$leave.Invoke($c,@([EventArgs]::Empty));$c.Refresh()
}
$form.TopMost=$true;$form.Activate();$form.Refresh();[Windows.Forms.Application]::DoEvents()
$screen=New-Object Drawing.Bitmap($form.Width,$form.Height)
$g=[Drawing.Graphics]::FromImage($screen)
try{$g.CopyFromScreen($form.Location,[Drawing.Point]::Empty,$form.Size);$screen.Save((Join-Path $PSScriptRoot ('..\build\starshine-live-'+$PreviewScale+'.png')))}finally{$g.Dispose();$screen.Dispose()}
$close.PerformClick();if(!$form.IsDisposed){throw 'Close failed'}
Write-Output "PASS: Starshine scale $PreviewScale; selection, Refresh, Details, bounds, disabled Stop, maximize/restore, minimize, close. No macro input sent."
