#include "cppdisplacementx/post.h"

#include <algorithm>

namespace cppdx {

static constexpr int32_t NORMAL_NEUTRAL = 127;

static uint8_t red_of(uint32_t p_pixel) {
	return (uint8_t)(p_pixel & 255u);
}

static uint8_t green_of(uint32_t p_pixel) {
	return (uint8_t)((p_pixel >> 8u) & 255u);
}

static uint8_t blue_of(uint32_t p_pixel) {
	return (uint8_t)((p_pixel >> 16u) & 255u);
}

static uint32_t alpha_bits_of(uint32_t p_pixel) {
	return p_pixel & 0xFF000000u;
}

static uint32_t pack_rgb(uint8_t p_r, uint8_t p_g, uint8_t p_b, uint32_t p_alpha_bits) {
	return (uint32_t)p_r | ((uint32_t)p_g << 8u) | ((uint32_t)p_b << 16u) | p_alpha_bits;
}

static uint8_t clamp_byte(int32_t p_value) {
	return (uint8_t)std::min(std::max(p_value, 0), 255);
}

static uint8_t lerp8(uint8_t p_from, uint8_t p_to, double p_ratio) {
	const double v = (double)p_from + ((double)p_to - (double)p_from) * p_ratio;
	if (v <= 0.0) {
		return 0;
	}
	if (v >= 255.0) {
		return 255;
	}
	return (uint8_t)(v + 0.5);
}

std::vector<ColorRgb> default_gradient() {
	return { ColorRgb{ 0, 255, 255 }, ColorRgb{ 149, 0, 255 }, ColorRgb{ 255, 229, 0 } };
}

Palette build_palette(const std::vector<ColorRgb> &p_stops) {
	const std::vector<ColorRgb> stops = p_stops.size() < 2 ? default_gradient() : p_stops;
	const size_t count = stops.size();
	const double denominator = (double)(count - 1);

	Palette palette;
	for (size_t x = 0; x < palette.r.size(); x++) {
		const double t = (double)x / 255.0;
		size_t segment = (size_t)(t * denominator);
		if (segment >= count - 1) {
			segment = count - 2;
		}
		const double position0 = (double)segment / denominator;
		const double position1 = (double)(segment + 1) / denominator;
		const double local = position1 > position0 ? (t - position0) / (position1 - position0) : 0.0;
		palette.r[x] = lerp8(stops[segment].r, stops[segment + 1].r, local);
		palette.g[x] = lerp8(stops[segment].g, stops[segment + 1].g, local);
		palette.b[x] = lerp8(stops[segment].b, stops[segment + 1].b, local);
	}
	return palette;
}

static uint32_t inverted_pixel(uint32_t p_pixel) {
	return pack_rgb((uint8_t)(255 - red_of(p_pixel)), (uint8_t)(255 - green_of(p_pixel)),
			(uint8_t)(255 - blue_of(p_pixel)), alpha_bits_of(p_pixel));
}

static uint32_t coloured_pixel(uint32_t p_pixel, const Palette &p_palette) {
	return pack_rgb(p_palette.r[red_of(p_pixel)], p_palette.g[green_of(p_pixel)],
			p_palette.b[blue_of(p_pixel)], alpha_bits_of(p_pixel));
}

void apply_invert(Canvas &r_canvas) {
	for (uint32_t &pixel : r_canvas.pixels) {
		pixel = inverted_pixel(pixel);
	}
}

void apply_color(Canvas &r_canvas, const Palette &p_palette) {
	for (uint32_t &pixel : r_canvas.pixels) {
		pixel = coloured_pixel(pixel, p_palette);
	}
}

void apply_invert_into(Canvas &r_destination, const Canvas &p_source) {
	for (size_t i = 0; i < p_source.pixels.size(); i++) {
		r_destination.pixels[i] = inverted_pixel(p_source.pixels[i]);
	}
}

void apply_color_into(Canvas &r_destination, const Canvas &p_source, const Palette &p_palette) {
	for (size_t i = 0; i < p_source.pixels.size(); i++) {
		r_destination.pixels[i] = coloured_pixel(p_source.pixels[i], p_palette);
	}
}

// OpenGL-style normal map from the height field's red channel; edge pixels
// clamp to themselves.
static void normal_into(std::vector<uint32_t> &r_out, const std::vector<uint32_t> &p_source, uint32_t p_width, uint32_t p_height) {
	for (uint32_t y = 0; y < p_height; y++) {
		const size_t row = (size_t)y * (size_t)p_width;
		const size_t up = y == 0 ? row : row - p_width;
		const size_t down = y == p_height - 1 ? row : row + p_width;
		for (uint32_t x = 0; x < p_width; x++) {
			const size_t left = x == 0 ? row + x : row + x - 1;
			const size_t right = x == p_width - 1 ? row + x : row + x + 1;
			const int32_t dx = (int32_t)red_of(p_source[left]) - (int32_t)red_of(p_source[right]);
			const int32_t dy = (int32_t)red_of(p_source[up + x]) - (int32_t)red_of(p_source[down + x]);
			r_out[row + x] = pack_rgb(clamp_byte(dx + NORMAL_NEUTRAL), clamp_byte(dy + NORMAL_NEUTRAL), 255, 0xFF000000u);
		}
	}
}

void apply_normal(Canvas &r_canvas) {
	std::vector<uint32_t> out(r_canvas.pixels.size(), 0u);
	normal_into(out, r_canvas.pixels, r_canvas.width, r_canvas.height);
	r_canvas.pixels = std::move(out);
}

void apply_normal_into(Canvas &r_destination, const Canvas &p_source) {
	normal_into(r_destination.pixels, p_source.pixels, p_source.width, p_source.height);
}

Canvas derive_map(const Canvas &p_field, OutputMode p_mode, bool p_invert, const std::vector<ColorRgb> &p_gradient) {
	Canvas derived(p_field.width, p_field.height);
	switch (p_mode) {
		case OutputMode::NORMAL:
			apply_normal_into(derived, p_field);
			break;
		case OutputMode::COLOR:
			apply_color_into(derived, p_field, build_palette(p_gradient));
			break;
		case OutputMode::GRAYSCALE:
			break;
	}
	if (p_invert) {
		if (p_mode == OutputMode::GRAYSCALE) {
			apply_invert_into(derived, p_field);
		} else {
			apply_invert(derived);
		}
		return derived;
	}
	if (p_mode == OutputMode::GRAYSCALE) {
		derived.pixels = p_field.pixels;
	}
	return derived;
}

} // namespace cppdx
