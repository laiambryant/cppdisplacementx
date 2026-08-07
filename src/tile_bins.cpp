#include "cppdisplacementx/tile_bins.h"

#include <algorithm>

namespace cppdx {

static uint32_t divide_rounding_up(uint32_t p_value, uint32_t p_divisor) {
	return (p_value + p_divisor - 1) / p_divisor;
}

template <typename TVisit>
static void for_each_touched_tile(const DrawCommand &p_cmd, uint32_t p_tile_cols, uint32_t p_tile_rows, TVisit p_visit) {
	if (p_cmd.clip_w <= 0 || p_cmd.clip_h <= 0) {
		return;
	}
	const uint32_t x0 = (uint32_t)std::max(p_cmd.clip_x, 0) / TILE;
	const uint32_t y0 = (uint32_t)std::max(p_cmd.clip_y, 0) / TILE;
	const uint32_t x1 = std::min((uint32_t)std::max(p_cmd.clip_x + p_cmd.clip_w - 1, 0) / TILE, p_tile_cols - 1);
	const uint32_t y1 = std::min((uint32_t)std::max(p_cmd.clip_y + p_cmd.clip_h - 1, 0) / TILE, p_tile_rows - 1);
	for (uint32_t ty = y0; ty <= y1; ty++) {
		for (uint32_t tx = x0; tx <= x1; tx++) {
			p_visit((size_t)(ty * p_tile_cols + tx));
		}
	}
}

TileBins bin_commands(const CommandList &p_commands, uint32_t p_width, uint32_t p_height) {
	TileBins bins;
	bins.tile_cols = divide_rounding_up(p_width, TILE);
	bins.tile_rows = divide_rounding_up(p_height, TILE);
	const size_t tile_count = (size_t)bins.tile_cols * (size_t)bins.tile_rows;

	std::vector<uint32_t> counts(tile_count, 0u);
	for (const DrawCommand &cmd : p_commands) {
		for_each_touched_tile(cmd, bins.tile_cols, bins.tile_rows, [&](size_t tile) { counts[tile]++; });
	}

	bins.ranges.assign(tile_count * 2, 0u);
	uint32_t offset = 0;
	for (size_t tile = 0; tile < tile_count; tile++) {
		bins.ranges[tile * 2] = offset;
		bins.ranges[tile * 2 + 1] = counts[tile];
		offset += counts[tile];
	}

	std::vector<uint32_t> cursor(tile_count, 0u);
	bins.indices.assign(offset, 0u);
	for (size_t command_index = 0; command_index < p_commands.size(); command_index++) {
		for_each_touched_tile(p_commands[command_index], bins.tile_cols, bins.tile_rows, [&](size_t tile) {
			bins.indices[bins.ranges[tile * 2] + cursor[tile]] = (uint32_t)command_index;
			cursor[tile]++;
		});
	}
	return bins;
}

} // namespace cppdx
