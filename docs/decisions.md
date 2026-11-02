# Decisions: first recorder/replay build

- C++20 core remains the implementation language. Existing portable LLVM toolchain is used; full MSVC/Qt packaging is still outstanding.
- Keep input and semantic game events separate. Ability clicks are not proof of successful casts; Off/On is currently replayed by its recorded click, not by setting a verified desired state.
- Store game geometry alongside each recording. Preserve map placement and initial unit conditions; never collect character position.
- Focus transitions release held inputs in playback and retain wall-clock delay. Do not record other application input.
- On playback, missing expected placement/upgrade/removal stops after three seconds. Do not retry a potentially duplicated placement.
- Native F8 latch prevents short presses being missed between memory polls.
- Same-camera timed-input playback is an experimental stepping stone, not the requested final adaptive, currency-aware engine.
- Selected-unit hotkeys confirmed by user: R = upgrade, X = sell. X is reserved for future sale actions and must not be sent merely because it is configured. Upgrade selects the owned unit by world position, sends R once, then verifies its upgrade in memory; it does not read the Upgrade panel.
- User now explicitly authorizes transient character XYZ reads for navigation; never save or display them. New --arm-strategy mode uses recorded waves as minimum prerequisites and semantic unit actions, not recorded mouse movement timing. Navigation currently uses bounded direct approach and camera orbit; blocked paths stop, full obstacle routing is not implemented.
