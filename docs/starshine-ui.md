# Starshine interface

The existing PowerShell / WinForms launcher retains its macro process handlers,
session storage, F8 stop mechanism, recording HUD, folders, prices editor and log.
The recorder and replay algorithms are unchanged by this redesign.

## Presentation

- `ui/Starshine.cs`: custom drawn form, vector four-point star, rounded buttons,
  hover/pressed/disabled states, record selector, status badge and record counts.
- `ui/ApplyTheme.ps1`: 900 x 470 layout, English labels matching the supplied
  reference, window controls and expandable Details area.
- `ui/CheckTheme.ps1`: non-gameplay UI smoke checks and preview rendering.
- `Launcher.ps1`: loads the presentation layer; retains existing event handlers.
- `Starshine.exe`: branded entry point, alongside compatible `AnimeMacro.exe`.

The window has a custom draggable title bar, minimize/maximize/close controls,
rounded boundary and system drop shadow. The gradient and wave decoration are
drawn vectors, not a screenshot background. Buttons are focusable native Button
subclasses and the record selector retains native keyboard/dropdown behavior.

Startup sizing uses system DPI and layout scales to window size. Preview checks
exercise factors 1, 1.25 and 1.5. These do not replace testing Windows display
settings on separate monitors; per-monitor DPI switching is not claimed.

## Verification

Run `powershell.exe -NoProfile -STA -ExecutionPolicy Bypass -File Launcher.ps1
-CheckOnly -PreviewScale 1.25` (on one line). This checks selection preservation,
Refresh, Details expansion/collapse, visible control bounds, idle Stop state,
maximize/restore, minimize and close. It renders previews in `build` and sends no
gameplay input. Repeat with 1 and 1.5.

Game recording/replay must be tested separately in Roblox. Visual checks do not
establish macro gameplay correctness. Existing strategy limitations remain.
