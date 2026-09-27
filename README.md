# Panelka

A procedural Eastern Bloc neighbourhood generator with a PS1-style renderer: vertex
wobble, affine texture warp, dithering and a low internal resolution. It is a raylib
C++ port of the three.js page and produces the same world for the same seed. It adds
walk-in interiors (stairwells and flats in the apartment blocks, churches and izbas),
and **Live** mode: a first-person, everyday-life sim of one citizen of Dubrava, capital
of the Érinska Narodna Poblacht. The setting is in [docs/SETTING.md](docs/SETTING.md).

## Live mode

Press **Live** (top right) in a Mikrorayon. You wake at 06:30 on a Luanek in your own
flat, with a kitchen, a bedroom and a combined bathroom. Your shift at the Tractor Works
starts at 08:00, and bus 14 takes 25 minutes from the stop at the edge of the district.

- **Needs:** hunger, thirst, energy, bladder, hygiene and mood (bottom left).
- **Using things:** look at something and press `E` for the first action, or `1`-`9`
  for the others. Actions take game time.
- **Your flat:** cook kasha or make tea at the stove (ingredients come from the fridge
  and cupboard in the same room), eat at the table, pack a lunch for work, drink and wash
  at the sinks, use the toilet, take a bath (hot water 06:00-09:00 and 18:00-23:00),
  sleep, switch lights on room by room, and put the wired radio on.
- **Out:** the kiosk sells bread, milk, tea, buckwheat and the paper, plus sugar, butter
  and sausage on ration coupons. The bus stop takes you to work: packed lunch or canteen
  soup at noon, late minutes docked, wages paid on Aoinek.
- **Tab** opens your notebook: who you are, your job, your money and what you carry.

A game minute passes each real second; actions run faster, and sleep and shifts
fast-forward. The rules live in `src/sim` and are tested headlessly (`ctest`).

## Building

You need CMake 3.16+, a C++17 compiler, and raylib's usual desktop dependencies
(on Debian/Ubuntu: `libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev libgl1-mesa-dev`).

```sh
cmake -S . -B build
cmake --build build -j
./build/panelka
```

By default CMake fetches raylib 6.0. To use an installed raylib (5.5 or newer)
instead, pass `-DPANELKA_SYSTEM_RAYLIB=ON`. The build copies `assets/` next to the
executable; the game also looks for `../assets/`.

## Controls

| Mode | Input |
| --- | --- |
| Fly (orbit) | drag to orbit · scroll to zoom · right-drag or Shift-drag to pan · `R` new seed · `H` hide the UI |
| Walk | `WASD` / arrows to move · drag to look · Shift to run |
| Live | as Walk, plus `E` / `1`-`9` to use things · `Tab` notebook · `Esc` closes a menu |

In walk mode, go up to any building to load its interior, then walk in through the entrance.

## Debug hooks

These environment variables are read at start-up. They are useful for reproducible screenshots.

| Variable | Effect |
| --- | --- |
| `PANELKA_SEED`, `PANELKA_MODE`, `PANELKA_STYLE`, `PANELKA_SEASON`, `PANELKA_HOUR`, `PANELKA_VIEW` | Initial scene settings (`MODE` is `district`, `old`, `village` or `single`) |
| `PANELKA_FX` | Toggle effects, e.g. `-dither+crt` (`snap affine dither crt fps30 shadows points orbit`) |
| `PANELKA_THEME` | `dark` or `light` UI (on Linux the default follows GNOME's colour scheme) |
| `PANELKA_AUTOSHOT=<file.png>` | Render about 40 frames, save a screenshot to the working directory, then exit |
| `PANELKA_WALK`, `PANELKA_ENTER=<rec>,<floor>,<spot>` | With `AUTOSHOT`: start in walk mode inside a building (spot `0` lobby, `1` landing, `2` flat, `3` church or izba) |
| `PANELKA_YAW`, `PANELKA_PITCH` | Extra camera rotation for `ENTER` |
| `PANELKA_POS=x,z,yaw,pitch[,feet]` | With `WALK`: stand at that world position (radians; `feet` is the floor height) |
| `PANELKA_LIVE=1` | With `AUTOSHOT`: start Live mode |
| `PANELKA_LIVE_RUN` | Run actions instantly first, e.g. `Toilet:1,Stove:1,Bus stop:1,wait:30,ui:notebook` (object name, 1-based action) |
| `PANELKA_LIVE_LOOK` | Stand facing the first home object of that name, e.g. `Stove` |
| `PANELKA_NUDGE=metres` | With `LIVE`: move the camera a tiny amount after placing it (render twice and diff to find flicker) |

Example: `PANELKA_SEED=4242 PANELKA_HOUR=22 PANELKA_AUTOSHOT=night.png ./build/panelka`

## Layout

| Path | Contents |
| --- | --- |
| `src/panelka.h` | Shared types: RNG, atlas tile ids, geometry builder, building records, interiors |
| `src/gen.cpp` | Geometry builder and every building, prop and scene generator |
| `src/interior.cpp` | Walk-in interiors, built on demand from the facade records |
| `src/atlas.cpp` | The procedural 512×512 texture atlas |
| `src/shaders.h` | GLSL 330 shaders (world, shadow depth, sky, particles, CRT overlay) |
| `src/main.cpp` | Window, rendering, UI, audio, walk mode, screenshot / OBJ export |
| `src/game.cpp` | Live mode: what you're looking at, running actions, HUD, notebook and menus |
| `src/ui.h` | Drawing helpers shared by `main.cpp` and `game.cpp` |
| `src/sim/` | The everyday-life rules: clock, needs, items, actions, work, kiosk (no raylib) |
| `tests/` | Headless tests for the sim (`ctest --test-dir build`) |
| `docs/SETTING.md` | The Érinska Narodna Poblacht: names, language, signage, goods and work |
| `assets/` | UI fonts: VT323 and Liberation Mono, both under the SIL Open Font License (see `assets/OFL-*.txt`) |
