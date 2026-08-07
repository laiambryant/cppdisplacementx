#ifndef CPPDX_POST_H
#define CPPDX_POST_H

#include "cppdisplacementx/canvas.h"

#include <array>
#include <cstdint>
#include <vector>

namespace cppdx {

struct ColorRgb {
	uint8_t r = 0;
	uint8_t g = 0;
	uint8_t b = 0;
};

// Which post-processing pass derives the final map from the rendered height
// field. The spellings match the sibling CLIs' --mode flag.
enum class OutputMode {
	GRAYSCALE = 0,
	NORMAL = 1,
	COLOR = 2,
};

struct Palette {
	std::array<uint8_t, 256> r{};
	std::array<uint8_t, 256> g{};
	std::array<uint8_t, 256> b{};
};

std::vector<ColorRgb> default_gradient();
Palette build_palette(const std::vector<ColorRgb> &p_stops);

void apply_invert(Canvas &r_canvas);
void apply_color(Canvas &r_canvas, const Palette &p_palette);
void apply_normal(Canvas &r_canvas);

void apply_invert_into(Canvas &r_destination, const Canvas &p_source);
void apply_color_into(Canvas &r_destination, const Canvas &p_source, const Palette &p_palette);
void apply_normal_into(Canvas &r_destination, const Canvas &p_source);

// Derives one emit's map from the shared height field without disturbing it,
// mirroring the bundle semantics of the sibling CLIs.
Canvas derive_map(const Canvas &p_field, OutputMode p_mode, bool p_invert, const std::vector<ColorRgb> &p_gradient);

} // namespace cppdx

#endif // CPPDX_POST_H
