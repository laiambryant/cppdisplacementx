# cppdisplacementx

Embeddable C++17 core of the Displacement X / JSplacement height-field
generator. It is the in-process sibling of
[godisplacementx](https://github.com/laiambryant/godisplacementx) (Go, CPU) and
[gpudisplacementx](https://github.com/laiambryant/gpudisplacementx) (Rust, wgpu):
same generator, no subprocess, no CLI.

It exists because a host that ships a game cannot spawn an executable. A
library links into the host binary, so runtime generation survives being
packaged — including on platforms where process spawning is not available at
all. Its first consumer is the
[procedural-city](https://github.com/laiambryant/procedural-city) Godot
extension, which references this repository as a submodule.

## What it is not

There is no image decoder, no file I/O, no rendering API and no threading
policy beyond `std::thread`. The library has **zero dependencies**: the host
decodes sprite PNGs, owns the GPU device, and writes the maps. That keeps the
same core usable from an engine extension, a CLI or a test harness.

## Layout

| Header | Responsibility |
|---|---|
| `rng.h` | The frozen PCG32 (XSH-RR 64/32) stream |
| `params.h` | Generator settings, composition-mode and sprite-pack tables |
| `command_list.h` | Draw-command generation — the only place randomness lives |
| `tile_bins.h` | 32x32 tile binning for the tiled compositor |
| `blend.h` | Integer-only blend math, mirror of the compute shader |
| `simd_row_blend.h` | SSE2 fast paths, exact rather than approximate |
| `cpu_compositor.h` | Ordered CPU backend |
| `composite_glsl.h` | Vulkan GLSL compute source for a GPU backend |
| `post.h` | Invert, gradient colouring, normal map |
| `sprite_atlas.h` | Flat atlas the host fills with decoded sprites |

## The determinism contract

One seed produces one city, on every backend and every machine. Three rules
keep that true, and all three are load-bearing:

1. **All randomness is in `command_list.cpp`.** Shaders and compositors consume
   no RNG; they replay a flat, data-only command list.
2. **The RNG stream is frozen.** The constants in `rng.cpp` are the same stream
   gpudisplacementx uses. Changing one changes every seeded output ever made.
   The stream is deliberately *not* godisplacementx's — the Go CLI is a
   different aesthetic lineage, not a byte-parity target.
3. **Every blend is integer-only.** No float appears between the command list
   and the finished pixel, so a GPU and a CPU agree bit for bit. `blend.h` and
   `composite_glsl.cpp` are line-for-line counterparts; editing one without the
   other silently breaks backend parity.

The SIMD paths are held to the same standard. `simd_row_blend.h` is enabled
only where the destination row is provably opaque and the mode collapses to
`out = div255(sa * blend + (255 - sa) * dst)`, where every intermediate stays
under 65536 and the 16-bit lanes reproduce the scalar result exactly. The test
suite compares the two across every supported mode.

## Building

The library is normally compiled straight into the host's build (nine .cpp
files, no configuration). For standalone work:

```sh
cmake -B build -S .
cmake --build build
ctest --test-dir build
```

## Licence

GPL-3.0, matching its sibling repositories.
