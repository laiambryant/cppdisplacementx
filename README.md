# cppdisplacementx

[![CI](https://github.com/laiambryant/cppdisplacementx/actions/workflows/ci.yml/badge.svg)](https://github.com/laiambryant/cppdisplacementx/actions/workflows/ci.yml)
[![C++17](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=cplusplus&logoColor=white)](https://en.cppreference.com/w/cpp/17)
![Platforms: Linux | Windows | macOS](https://img.shields.io/badge/platforms-Linux%20%7C%20Windows%20%7C%20macOS-lightgrey)
![Dependencies: none](https://img.shields.io/badge/dependencies-none-brightgreen)
[![License: GPL v3](https://img.shields.io/badge/License-GPLv3-blue.svg)](LICENSE)

Embeddable C++17 core of the Displacement X / JSplacement displacement-map
generator. It links straight into a host binary and renders into memory: there
is no CLI, no subprocess, and no file I/O. Outside of `<emmintrin.h>` on x86,
it includes nothing but the standard library.

It is the in-process sibling of
[godisplacementx](https://github.com/laiambryant/godisplacementx) (Go, CPU) and
`gpudisplacementx` (Rust, wgpu). All three share one RNG stream and one
definition of the blend math, so a given seed produces the same image in every
one of them.

## Samples

| | |
|:--:|:--:|
| ![Grayscale height field](docs/images/sample-grayscale.png) | ![Colour gradient](docs/images/sample-color.png) |
| `OutputMode::GRAYSCALE` | `OutputMode::COLOR` |
| ![Normal map](docs/images/sample-normal.png) | ![Dense layering](docs/images/sample-dense.png) |
| `OutputMode::NORMAL` | Heavier layering, `OutputMode::COLOR` |

The first three are one 320x320 height field put through the three output
modes, not three separate renders. The fourth is a second field, drawn with
more iterations and a wider composition-mode mask.

## Integrating

The library is nine `.cpp` files with no configuration to speak of, so the
usual approach is to compile them into the host's own build and put `include/`
on the include path. C++17 and a threads library are the only requirements.

With CMake, either vendor the repository:

```cmake
add_subdirectory(third_party/cppdisplacementx)
target_link_libraries(my_host PRIVATE cppdisplacementx)
```

or fetch it at configure time:

```cmake
include(FetchContent)
FetchContent_Declare(cppdisplacementx
    GIT_REPOSITORY https://github.com/laiambryant/cppdisplacementx.git
    GIT_TAG        v1.0.0)
FetchContent_MakeAvailable(cppdisplacementx)
target_link_libraries(my_host PRIVATE cppdisplacementx)
```

Tagged releases also ship a prebuilt `include/` + `lib/` tree per platform.

## Usage

Rendering is two steps: composite a height field, then derive a map from it.

```cpp
#include "cppdisplacementx/engine.h"

using namespace cppdx;

Params params;
params.iterations = 200;
params.composition_mode_mask = (1u << MODE_SOURCE_OVER) | (1u << MODE_DIFFERENCE);

// Sprite layers are off by default, so this render needs no atlas contents.
SpriteAtlas atlas;
atlas.make_empty_if_unused();

// A thread count of 0 asks for hardware_concurrency.
const Canvas field = render_field_cpu(params, 1024, 1024, /*seed=*/0xC0FFEEu, atlas, 0);
const Canvas map = derive_map(field, OutputMode::NORMAL, /*invert=*/false, default_gradient());
```

`Canvas::pixels` holds `width * height` words packed as
`r | g << 8 | b << 16 | a << 24`, which is straight-alpha RGBA8 in memory
order. Hand that buffer to whatever the host already uses to encode images or
upload textures; the library will not write it anywhere.

Deriving several maps from one field is cheap and leaves the field untouched,
so a single render can emit a grayscale map, a normal map and a colour map
together:

```cpp
const Canvas heightmap = derive_map(field, OutputMode::GRAYSCALE, false, default_gradient());
const Canvas normal = derive_map(field, OutputMode::NORMAL, false, default_gradient());
const Canvas tinted = derive_map(field, OutputMode::COLOR, false, my_gradient_stops);
```

## Sprites

The library decodes nothing, so enabling sprite layers means filling the atlas
first. Draw commands index it through the pack tables rather than through
whatever the atlas happens to hold, which makes the count a hard requirement:
supply exactly `sprite_count(sprite_pack_mask)` sprites, in canonical pack
order, or the compositor will index past the end of the atlas.

```cpp
params.sprites_enabled = true;
params.sprite_pack_mask = (1u << PACK_CLASSIC) | (1u << PACK_BIGDATA);

SpriteAtlas atlas;
for (const uint32_t pack : selected_sprite_packs(params.sprite_pack_mask)) {
    for (uint32_t i = 0; i < sprite_pack_length(pack); i++) {
        atlas.add_sprite(my_decoder(sprite_pack_name(pack), i), side); // square, RGBA8
    }
}
assert(atlas.sprite_count() == sprite_count(params.sprite_pack_mask));
```

`make_empty_if_unused()` covers the other case: it puts a 1x1 placeholder in an
atlas that is still empty, so a render with `sprites_enabled = false` has
something valid to point at. It is not a substitute for loading the packs.

## Rendering on the GPU

`render_field_cpu` is a convenience wrapper over three pieces the host can also
drive itself, which is what a GPU backend does. The library generates the work
and hands over the shader source; the host owns the device.

```cpp
#include "cppdisplacementx/composite_glsl.h" // not pulled in by engine.h

const CommandList commands = build_command_list(params, width, height, seed);
const TileBins bins = bin_commands(commands, width, height);
const char *glsl = composite_compute_glsl();
```

`commands` uploads as raw bytes (each `DrawCommand` is a fixed 16 x `uint32_t`,
the same struct the shader declares). `bins` gives the per-tile command lists
for the 32x32 tiling, and `atlas.pixel_data()` / `atlas.meta_data()` upload
verbatim. The compute source expects, in set 0: `0` canvas (read/write),
`1` commands, `2` atlas, `3` sprite meta, `4` bin indices, `5` tile ranges,
with `canvas_w`, `canvas_h` and `tile_cols` as push constants.

## Determinism

One seed produces one output, on either backend and on any machine. Three
properties hold that together:

1. Every random draw happens in `command_list.cpp`. The compositors and the
   shader replay a flat, data-only command list and consume no RNG, so they
   cannot drift apart by sampling in a different order.
2. The PCG32 (XSH-RR 64/32) stream in `rng.cpp` is byte-identical to
   gpudisplacementx's and is frozen. Changing a constant in it would change
   every seeded output ever made.
3. Every blend is integer-only, so there is no floating-point rounding to
   diverge on. `blend.h` and `composite_glsl.cpp` are line-for-line
   counterparts, and the SSE2 paths in `simd_row_blend.h` are exact rather
   than approximate.

`tests/parity_tests.cpp` checks all three: the RNG against its fixed stream,
the command list against repeated seeding, and the SIMD rows against the scalar
path for every supported mode.

## Headers

| Header | Responsibility |
|---|---|
| `engine.h` | `render_field_cpu`, the one-call CPU path |
| `params.h` | Generator settings, composition-mode and sprite-pack tables |
| `rng.h` | The frozen PCG32 stream |
| `command_list.h` | Draw-command generation, and the only place randomness lives |
| `draw_command.h` | The 64-byte command struct shared with the shader |
| `canvas.h` | RGBA8 pixel buffer |
| `blend.h` | Integer-only blend math, mirror of the compute shader |
| `simd_row_blend.h` | SSE2 fast paths, guarded and exact |
| `cpu_compositor.h` | Ordered, multithreaded CPU backend |
| `tile_bins.h` | 32x32 tile binning for the tiled shader |
| `composite_glsl.h` | Vulkan GLSL compute source for a GPU backend |
| `post.h` | Invert, gradient colouring, normal map |
| `sprite_atlas.h` | Flat atlas the host fills with decoded sprites |

## Building the tests

```sh
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

CI builds and runs this suite on Linux, Windows and macOS for every push and
pull request against `main`.

## Releases

Pushing a tag matching `v*.*.*` runs the full test matrix first, then builds,
installs and packages the library on all three platforms and publishes a GitHub
Release with one archive per platform:

```sh
git tag v1.0.1
git push origin v1.0.1
```

## Licence

GPL-3.0, matching its sibling repositories. See [LICENSE](LICENSE).

## Credits

Part of the Displacement X family, a C++/Go/Rust reimplementation of
[satelllte/displacementx](https://github.com/satelllte/displacementx).
