# Starshine

Experimental Windows launcher, recorder and Anime Adventures macro. C++20 read-only memory adapter with normal Windows keyboard/mouse input; no memory writes or injection.

## Install

Download **Starshine-win-x64.zip** from [Releases](https://github.com/a11yboop/Starshine/releases), extract it to a writable folder and run **Starshine.exe**. Requires Windows x64 and built-in Windows PowerShell 5.1. Keep the files together.

## Updates and version selection

The launcher checks GitHub Releases in the background. Click **Versions**, select a published version and click **Install selected version**. The launcher downloads the package, verifies SHA-256 checksums, closes, installs and restarts. You can choose an earlier compatible release in the same list. The first stable release is v0.5.2; additional choices appear when more releases are published.

Updating requires the macro to be stopped. Records, logs, per-record lobby settings and custom upgrade prices are retained. Versioned Roblox offsets and unit/map definitions are updated with the program. Replaced files are backed up under `.updates`; installation failures trigger rollback. Backups are retained. Older releases may not understand recordings produced by newer versions; keep your recordings backed up before downgrading. The updater does not bypass Roblox version mismatches.

## Macro

Record and replay through the launcher. **F8 stops**. Focus loss stops input. R upgrades a selected unit. Experimental autonomous play is currently a developer CLI mode (`--autoplay PID config/layout.json RECORDING_DIRECTORY`), not a guaranteed map solver. It prioritizes a farm, defense near the leading enemy, then farm upgrades before defender upgrades. Navigation follows enemy road segments. Ground geometry uses bounding-box estimates; unsupported maps and missing data stop the run.

Roblox updates can require new offsets in `config/layout.json`. Complete game runs, arbitrary maps, ability-state automation and compatibility after Roblox updates are not guaranteed.

## Build and test

Run `build.ps1` with llvm-mingw 20261020 extracted under `.tools/llvm-mingw-20261020-ucrt-x86_64`. This builds the native launcher/recorder and runs core tests. UI check: `powershell -NoProfile -STA -File Launcher.ps1 -CheckOnly`. Package: `powershell -File scripts/Package.ps1 -Version 0.5.2`. Update integration tests: `powershell -File tests/updater_tests.ps1 -Package <release-folder>`.

To publish another release, update `app-version.json`, build, test and package the same version, then attach `Starshine-win-x64.zip` and its `.sha256` file to a GitHub release tagged `vX.Y.Z`. Only non-prerelease releases with both assets appear in the updater. Release packages contain code and defaults only, never personal recordings.

Vendored nlohmann/json is distributed under its MIT license in `third_party/json-LICENSE.MIT`. Historical implementation notes are in `docs/legacy-recorder.md` and may describe older limitations.
