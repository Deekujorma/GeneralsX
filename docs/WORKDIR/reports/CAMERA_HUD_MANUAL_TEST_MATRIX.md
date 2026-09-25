# Camera and HUD Manual Test Matrix

## Camera

- [ ] Confirm the fresh/default wheel zoom reaches 750, substantially farther than the former 500 limit.
- [ ] Apply maximum zoom values 500, 750, and 1000; verify wheel zoom-in/out and safe settling when reducing the maximum.
- [ ] Inspect all map edges and both high and low terrain; verify the camera never clips below terrain.
- [ ] Exercise mouse and keyboard camera movement in campaign and skirmish.
- [ ] Play a replay and verify existing recorded-camera behavior remains intact.

## Battlefield HUD

- [ ] Check 60%, 75%, and 100% at each supported resolution.
- [ ] Check USA, China, GLA, and all Zero Hour general variants.
- [ ] Select units and buildings; exercise construction, production queues, upgrades, and generals powers.
- [ ] Verify minimap, portrait, selected-unit information, money, power, idle worker, options, and general buttons.
- [ ] Verify every visual button is clickable at its displayed location and tooltips remain readable.
- [ ] Toggle DEFAULT to LOW and back; verify DEFAULT restores the configured scale.
- [ ] Change resolution and player/faction scheme, then verify geometry and artwork remain aligned.
- [ ] Restart after Apply and verify camera, terrain distance, and HUD scale persistence.
