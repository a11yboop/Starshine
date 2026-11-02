# Anime Adventures macro — experimental v0.3

C++20 recorder and experimental timed-input playback. This is not yet the complete Qt/MSVC application or an adaptive tower-defense player.

## Use

1. Enter a match, choose the camera, close chat and arrange the interface.
2. Run Record.cmd. Map geometry is captured before recording starts.
3. Play in the Roblox window. Gameplay keys, mouse moves, wheel and button transitions are captured alongside owned-unit state changes.
4. Press F8 before switching to another window. This flushes recording.json and input.json. Stop.cmd also stops capture. Leaving Roblox records a release-all boundary; inactive time is retained, and input in other applications is excluded.
5. Replay.cmd opens a recording selector and arms playback for up to three minutes. Restore the same map, starting wave, existing unit positions/upgrades, camera, interface, window size, unit hotbar and game speed, then focus Roblox. Direct playback without -WaitForMatch instead allows five seconds to focus Roblox.
6. F8 stops playback. Focus loss, window resizing, map change or read failure abort playback and release generated held keys/buttons.

## Files

Each recordings/session-* folder has geometry.json, events.jsonl, input.jsonl, status.json, and after normal stop recording.json and input.json. JSONL journals are flushed as capture progresses. Geometry includes available map/road/terrain/base part positions, sizes and rotations. Meshes are bounding boxes, not exact meshes or verified placement masks. No character position, username, geographic location or timezone is saved.

## What works and what remains

Memory events: initial-unit baseline, new owned units, upgrades, disappearance classified as sale by the user's rule, wave and read gaps. Units with identical names get separate IDs. Intermediate upgrades between 200 ms polls can be missed and are marked when observed as jumps. A disappearance is inferred sale, not game-confirmed sale.

Input: gameplay keys W/A/S/D/Q/E/R/F, 1–6, arrows, Shift and Space; mouse buttons, moves and wheel, only while Roblox is foreground. Do not type into game chat while recording. Other application input is excluded. Clicks on ability buttons and Off/On are captured as clicks; they are not decoded as confirmed ability activations or switch states. Ability duration and remaining cooldown remain unverified.

Playback: timed input via SendInput, with initial memory preflight, matching client size/map/wave/owned-unit baseline. It verifies recorded unit appearances, upgrade targets and removals using memory checkpoints with a three-second timeout. It does not adapt camera, detect occlusion, wait for currency, align to future waves, enter maps from lobby, or guarantee match completion. Mouse-camera behavior and full action replay require live testing. Recordings are marked replayReady=false; inputReplayEligible only means the experimental input stream passed basic capture conditions. This is not an autonomous replay-ready strategy config.

## Build / test

Run build.ps1. Compiler: llvm-mingw 20261020, C++20 static build; vendored nlohmann/json 3.12.0. CMakeLists.txt also supports MSVC configuration; MSVC/Qt packaging has not been tested. recorder.exe --self-test covers unit identity, baseline, upgrades, missing levels, read gaps, inferred sale, re-placement and map changes. Replay.ps1 -Session <folder> -ValidateOnly runs preflight without sending input.

Read-only adapter is pinned to version-5a21b87bbc2641cf. No memory writes or injection. Native hooks capture input in the recorder process; they are not injected into Roblox. See Microsoft documentation: https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-sendinput and https://learn.microsoft.com/en-us/windows/win32/winmsg/lowlevelmouseproc .

Validation: C++ build and unit tests passed. Empty/incomplete input recording is rejected. New live match capture read 16,015 geometry parts with zero reported read errors and no queue truncation. Real playback, abilities and Off/On transitions still need live verification.

## v0.3 verification
F8 uses a latched keyboard-hook event rather than the 200 ms memory loop. Graceful shutdown joins the input thread before final serialization. Recordings with focus transitions include release_all and focus_resumed; playback preserves inactive time and does not switch applications. A live 34-second test saved Bulma placement and upgrade 0 to 1, and F8 stopped successfully. Playback must still pass the live test. Existing v0.2 recordings are not silently reclassified.


## Vote start and Replay

Replay.cmd now arms playback. While waiting, it can click ResultsUI/Holder/Buttons/Next and ResultsUI/Finished/NextRetry once each, only when the screen and every ancestor frame are visible and Roblox is foreground. Each button must retain the same identity and visible flags for 1.2 seconds before the cursor moves, followed by 350 ms of hover and the input sender's 80 ms pre-click check. Hidden/recreated buttons, read failures and polling gaps reset readiness. Visibility is inferred from UI flags; actual rendering or animation completion is not directly measured.

On a fresh match it waits up to 45 seconds for VoteStart/Holder/ButtonHolder/Yes to become ready, clicks once, then starts a separate 45-second wait for a positive wave number. During hover and immediately before pressing, it rechecks the button, focus, client size, map and wave. If the wave starts automatically, the vote click is skipped. F8 cancels all waits. A sent click is not treated as successful start until the wave changes. The readiness timing self-tests cover delayed appearance, transient visibility, button replacement and polling gaps; live results are described below.

These three button centers are calibrated from user-supplied 1920x1080 screenshots. Another client size is rejected; automatic UI scaling/calibration is not implemented. The displayed hierarchy and visibility flags were inspected in the live client. Live clicks and successful round-trip replay remain unverified. This handles one restart while arming; it does not implement an unbounded post-match farming loop. Playback timing still uses the recorded input timeline after the confirmed start, so it may not align to the recording's original wave timing.

## System cursor click path

Mouse replay moves the actual desktop cursor using SendInput after ClientToScreen. Before a fresh mouse-down or wheel event, it refreshes hover with a nonzero relative movement and return, verifies the resulting endpoint, waits 80 ms, then checks foreground and the topmost window under the pointer. Lifecycle buttons perform the refresh before their 350 ms hover wait and send down/up without moving the pointer again. Relative refresh does not change Windows pointer settings and is skipped while a mouse button is held. This is standard synthesized Windows input, not hardware input. It does not guarantee compatibility with camera cursor-lock/raw-input modes; a locked or displaced pointer cancels a click instead of guessing.

## Hover-input correction

SetCursorPos and absolute SendInput movement alone did not establish reliable in-game hover. A live diagnostic then tested absolute positioning followed by a relative one-count movement and return: the cursor changed to a hand, and button-only clicks on Replay and Vote Yes were followed by a new match and positive wave number. This motivated the current relative refresh and separate lifecycle button transitions. It does not prove full action replay or compatibility with every input mode. Coordinate normalization tests cover negative virtual-screen origins and range rejection; effective per-monitor DPI awareness is checked at startup. Earlier claims that Next succeeded automatically were withdrawn after the user clarified that they clicked it manually.

The subsequent recorder run traversed Next/Replay into a fresh match and confirmed a positive wave after sending Vote Yes. The final verification logged the first wave 1,187 ms after the vote input (`recordings/relative-hover-final-output.txt`). It then stopped at the Bulma placement checkpoint (`unit_discovered_after_gap / bulma_summer:golden`); the complete recording has not passed replay. Positive wave confirms the match began; timing is logged to distinguish prompt response from the automatic countdown. Recorded mouse-up avoids repositioning when already at its recorded point, and F8 is rechecked during hover refresh and immediately before playback button input.

## Wave-based unit strategy (experimental)
Replay-Strategy.cmd runs placements/upgrades using recorded wave as a minimum prerequisite, with currency waits, instead of old input timing. It does not replay abilities, toggles, sales or their timing. Original Replay.cmd remains available. Navigation reads character XYZ transiently, walks directly using current camera axes and attempts camera orbit; blocked paths stop. Full obstacle routing and arbitrary camera/view handling are not verified. Upgrade cost for Bulby 0-to-1 is currently configured as 1500 from the observed game UI in config/upgrade-costs.json; other prices must be configured. Lifecycle button centers now come from UI memory rectangles. New strategy/navigation still need live validation.

## Launcher v0.1
Start AnimeMacro.exe beside Launcher.ps1, or use the self-contained folder dist/AnimeMacro-v0.1. The native Windows Forms launcher offers recording selection, recording, wave-strategy playback, cooperative stop, and logs. Closing during a run requests stop and waits for graceful exit. F8 remains available in Roblox. The launcher uses built-in Windows PowerShell; Qt packaging is not implemented. The packaged example contains the verified Bulby placement/upgrade recording. Navigation and complete strategy playback still require live validation.
