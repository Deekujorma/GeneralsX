# Configurable Keyboard Controls Manual Test Matrix

## Scope

Validate both Generals and Zero Hour on Linux, Linux Flatpak, macOS, and Windows where available. Use a clean user-data directory first, then repeat with custom bindings.

## Matrix

1. Open the retail Options screen, confirm the injected Keyboard Controls button is visible, open the GeneralsX keyboard layout, then confirm a clean install pans with Up, Down, Left, and Right Arrow.
2. Rebind Camera Pan Up/Left/Down/Right to W/A/S/D in Keyboard Options, accepting conflicts when prompted.
3. Hold and release each direction repeatedly, including rapid press/release cycles.
4. Verify W+A, W+D, S+A, and S+D diagonal scrolling.
5. Hold opposite directions together and confirm they cancel on that axis; release either and confirm the remaining direction resumes.
6. Hold a movement key, press Shift, release the movement key, then release Shift; confirm scrolling stops.
7. Open and close gameplay menus around key transitions and confirm no held direction survives the input reset.
8. Select a command, click the capture field, and rebind it repeatedly; verify focus, pending text, current binding, Assign, and Escape cancellation after each attempt.
9. Create a conflict and test both Cancel and Replace; verify Replace unbinds the overlapping command only.
10. Verify W, Shift+W, Ctrl+W, and Alt+W are distinct; specifically bind W to camera pan and Shift+W to a held BEGIN/END action, then verify both release correctly.
11. Focus text-entry fields and confirm gameplay controls do not activate.
12. Confirm the selected-command description displays the reset/unbind shortcuts; press Ctrl+Delete while capturing to unbind, then Ctrl+Backspace to restore its default.
13. Use Reset All and confirm all retail/mod effective defaults are restored.
14. Restart and confirm custom bindings persist in `KeyBindings.ini` under the engine user-data directory.
15. Corrupt one override and add an obsolete action name; confirm valid overrides still load and the game remains usable.
16. Confirm mouse-edge and right-mouse scrolling, scroll speed, constraints, campaign play, replay playback, and network determinism are unchanged.
