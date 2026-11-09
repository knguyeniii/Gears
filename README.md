# Holdfast

An original third-person cover shooter vertical slice for Unreal Engine 5 (C++ only).

You are the last warden on shift at a freight yard when the yard's salvage machines
come through the east wall. Hold the yard through three breaches.

This is **not** a recreation of any existing game. It is an original setting, cast and
look, built around genre mechanics: over-the-shoulder camera, snap-to-cover, pop-up and
corner-peek aiming, blind fire, vaulting, a dodge roll, regenerating health and waves of
enemies that fight from cover.

## Status

- **Never compiled or run.** It was written in an environment with no Unreal Engine
  install. Expect to fix some compile errors and tune numbers on first build.
- **Greybox visuals.** Everything (level, characters, weapons, tracers) is assembled at
  runtime from the engine's basic shapes. There are no meshes, animations, textures,
  audio or VFX assets in the repo. Lighting uses Lumen, sky atmosphere, volumetric fog
  and a graded camera, but this is a gameplay prototype, not a finished-looking game.

## Running it

1. Install Unreal Engine 5.5 (5.3+ should work; change `EngineAssociation` in
   `Holdfast.uproject` if needed) with a C++ toolchain.
2. Right-click `Holdfast.uproject` and generate project files, then build the
   `HoldfastEditor` target, or just open the `.uproject` and let it compile.
3. Press Play. The project starts on the engine's empty `Entry` map; the game mode
   builds the whole yard, lighting and encounter in code.

## Controls

| Action | Keyboard / mouse | Gamepad |
| --- | --- | --- |
| Move / look | WASD / mouse | Left / right stick |
| Sprint | Left Shift (hold) | Click left stick |
| Take cover, leave cover | Space | A / Cross |
| Vault low cover | Space while pushing into it | same |
| Dodge roll | Space while moving, away from cover | same |
| Aim (pop up / lean out in cover) | Right mouse | Left trigger |
| Fire (blind fire from low cover without aiming) | Left mouse | Right trigger |
| Reload | R | X / Square |
| Swap weapon | Q or mouse wheel | Y / Triangle |
| Melee | F | B / Circle |

If vertical look is inverted, flip `bInvertLookY` in `HFPlayerCharacter.h`.

## Code map (`Source/Holdfast`)

| File | Purpose |
| --- | --- |
| `HFGameMode` | Spawns the arena, sun, sky, fog and player start; runs the three-breach encounter and restart. |
| `HFArena` | Builds the yard geometry, tags cover, generates AI cover slots, grid A* pathfinding. |
| `HFCharacterBase` | Health, weapons, hitscan fire with spread, falloff and headshots, primitive body. |
| `HFPlayerCharacter` | Camera, polled input, cover system, vault, roll, melee, health regen. |
| `HFEnemyCharacter` | Drone (cover fighter), Skitter (melee rusher), Hauler (armoured suppressor). |
| `HFHUD` | Reticle, hit markers, ammo, damage vignette, banners. |
| `HFTracer` | Shot streaks. |
