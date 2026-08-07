#ifndef CPPDX_PARAMS_H
#define CPPDX_PARAMS_H

#include <cstdint>
#include <vector>

namespace cppdx {

struct DualRange {
	int64_t min = 0;
	int64_t max = 0;
};

enum CompositionMode : uint32_t {
	MODE_COLOR_BURN = 0,
	MODE_COLOR_DODGE = 1,
	MODE_DARKEN = 2,
	MODE_DIFFERENCE = 3,
	MODE_EXCLUSION = 4,
	MODE_HARD_LIGHT = 5,
	MODE_LIGHTEN = 6,
	MODE_LIGHTER = 7,
	MODE_LUMINOSITY = 8,
	MODE_MULTIPLY = 9,
	MODE_OVERLAY = 10,
	MODE_SCREEN = 11,
	MODE_SOFT_LIGHT = 12,
	MODE_SOURCE_ATOP = 13,
	MODE_SOURCE_OVER = 14,
	MODE_XOR = 15,
	COMPOSITION_MODE_COUNT = 16,
};

enum SpritePack : uint32_t {
	PACK_CLASSIC = 0,
	PACK_BIGDATA = 1,
	PACK_AGGROMAXX = 2,
	PACK_CRAPPACK = 3,
	SPRITE_PACK_COUNT = 4,
};

// The mask bit order is the canonical name order both sibling CLIs serialize:
// bit i of composition_mode_mask is composition_mode_name(i).
const char *composition_mode_name(uint32_t p_code);
const char *sprite_pack_name(uint32_t p_pack);
uint32_t sprite_pack_length(uint32_t p_pack);

struct Params {
	int64_t iterations = 100;
	int64_t background_brightness = 32;

	bool rect_enabled = true;
	DualRange rect_brightness{ 0, 255 };
	DualRange rect_alpha{ 50, 100 };
	int64_t rect_scale = 100;

	bool grid_enabled = true;
	DualRange grid_brightness{ 0, 255 };
	DualRange grid_alpha{ 80, 100 };
	int64_t grid_scale = 100;
	DualRange grid_amount{ 2, 5 };
	int64_t grid_gap = 100;

	bool cols_enabled = true;
	DualRange cols_brightness{ 0, 255 };
	DualRange cols_alpha{ 80, 100 };
	int64_t cols_scale = 100;
	DualRange cols_amount{ 2, 5 };
	int64_t cols_gap = 100;

	bool rows_enabled = true;
	DualRange rows_brightness{ 0, 255 };
	DualRange rows_alpha{ 80, 100 };
	int64_t rows_scale = 100;
	DualRange rows_amount{ 2, 5 };
	int64_t rows_gap = 100;

	bool lines_enabled = true;
	DualRange lines_brightness{ 0, 255 };
	DualRange lines_alpha{ 80, 100 };
	DualRange lines_width{ 5, 10 };

	bool sprites_enabled = false;
	uint32_t sprite_pack_mask = 1u << PACK_CLASSIC;
	bool sprites_rotation_enabled = true;
	bool seamless = false;
	uint32_t composition_mode_mask = 1u << MODE_SOURCE_OVER;
};

// An empty mask resolves to a single source-over entry, mirroring what the
// sibling CLIs receive: the draw loop always consumes one RNG pick per
// iteration, so a silently empty list would desynchronise the stream.
std::vector<uint32_t> composition_mode_codes(uint32_t p_mask);
std::vector<uint32_t> selected_sprite_packs(uint32_t p_mask);
uint32_t sprite_count(uint32_t p_mask);

} // namespace cppdx

#endif // CPPDX_PARAMS_H
