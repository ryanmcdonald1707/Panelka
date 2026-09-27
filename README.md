# Panelka

A procedural Eastern Bloc neighbourhood generator with a PS1-style renderer: vertex
wobble, affine texture warp, dithering and a low internal resolution. It is a raylib
C++ port of the three.js page and produces the same world for the same seed. It adds
walk-in interiors: stairwells and flats in the apartment blocks, churches and izbas.

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
| `assets/` | UI fonts: VT323 and Liberation Mono, both under the SIL Open Font License (see `assets/OFL-*.txt`) |
