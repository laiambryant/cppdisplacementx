#ifndef CPPDX_TILE_BINS_H
#define CPPDX_TILE_BINS_H

#include "cppdisplacementx/draw_command.h"

#include <cstdint>
#include <vector>

namespace cppdx {

// Must match TILE in composite.glsl.
inline constexpr uint32_t TILE = 32;

// Per tile, the indices of the commands whose clipped rect touches it, in
// submission order. The tiled shader walks one tile's list per pixel, so the
// per-pixel blend sequence stays the submission sequence.
struct TileBins {
	uint32_t tile_cols = 0;
	uint32_t tile_rows = 0;
	std::vector<uint32_t> ranges;
	std::vector<uint32_t> indices;
};

TileBins bin_commands(const CommandList &p_commands, uint32_t p_width, uint32_t p_height);

} // namespace cppdx

#endif // CPPDX_TILE_BINS_H
