#include "cppdisplacementx/command_list.h"

#include "cppdisplacementx/rng.h"

#include <algorithm>
#include <cmath>

namespace cppdx {

// Structural formulas ported from godisplacementx's draw.go. Together with the
// RNG stream they define the seeded-output contract, so they are frozen.
static constexpr double RECT_SIDE_MIN_DIVISOR = 16.0;
static constexpr double RECT_SIDE_MAX_DIVISOR = 8.0;
static constexpr double CELL_SIDE_MIN_DIVISOR = 256.0;
static constexpr double CELL_SIDE_MAX_DIVISOR = 16.0;
static constexpr double SPRITE_SIDE_MIN_DIVISOR = 32.0;
static constexpr double SPRITE_SIDE_MAX_DIVISOR = 2.0;
static constexpr double OFFSCREEN_MARGIN_DIVISOR = 16.0;
static constexpr double LINE_THICKNESS_REFERENCE_SIDE = 2500.0;
static constexpr int64_t ELONGATION_MAX = 10;
static constexpr double PERCENT = 100.0;
static constexpr int64_t ALPHA_OPAQUE = 100;
static constexpr int64_t GRAY_MAX = 255;
static constexpr int64_t QUARTER_TURN_DEGREES = 90;
static constexpr int64_t MAX_QUARTER_TURNS = 3;
static constexpr int64_t LAYER_KIND_COUNT = 6;

static int64_t round_half_up(double p_value) {
	return (int64_t)std::floor(p_value + 0.5);
}

static int64_t clamp_i64(int64_t p_value, int64_t p_low, int64_t p_high) {
	return std::min(std::max(p_value, p_low), p_high);
}

static int32_t saturating_add_i32(int32_t p_a, int32_t p_b) {
	const int64_t sum = (int64_t)p_a + (int64_t)p_b;
	return (int32_t)clamp_i64(sum, INT32_MIN, INT32_MAX);
}

static int64_t origin_min(bool p_seamless, double p_span) {
	return p_seamless ? 0 : round_half_up(-p_span / OFFSCREEN_MARGIN_DIVISOR);
}

class Builder {
public:
	Builder(int64_t p_width, int64_t p_height) :
			width(p_width), height(p_height) {}

	void push_fill(int64_t p_x, int64_t p_y, int64_t p_w, int64_t p_h, int64_t p_gray, int64_t p_alpha, uint32_t p_mode) {
		if (p_w <= 0 || p_h <= 0 || p_alpha <= 0) {
			return;
		}
		DrawCommand cmd = {};
		cmd.kind = CMD_FILL_RECT;
		cmd.mode = p_mode;
		cmd.x = (int32_t)p_x;
		cmd.y = (int32_t)p_y;
		cmd.w = (int32_t)p_w;
		cmd.h = (int32_t)p_h;
		cmd.gray = (uint32_t)clamp_i64(p_gray, 0, GRAY_MAX);
		cmd.alpha = (uint32_t)clamp_i64(p_alpha, 0, ALPHA_OPAQUE);
		push_clipped(cmd);
	}

	void push_sprite(int64_t p_x, int64_t p_y, int64_t p_size, uint32_t p_sprite, int64_t p_rotation, uint32_t p_mode) {
		if (p_size <= 0) {
			return;
		}
		DrawCommand cmd = {};
		cmd.kind = CMD_SPRITE;
		cmd.mode = p_mode;
		cmd.x = (int32_t)p_x;
		cmd.y = (int32_t)p_y;
		cmd.w = (int32_t)p_size;
		cmd.h = (int32_t)p_size;
		cmd.alpha = (uint32_t)ALPHA_OPAQUE;
		cmd.sprite = p_sprite;
		cmd.rot = (uint32_t)p_rotation;
		push_clipped(cmd);
	}

	int64_t canvas_width() const { return width; }
	int64_t canvas_height() const { return height; }
	CommandList take() { return std::move(commands); }

private:
	void push_clipped(DrawCommand p_cmd) {
		const int32_t x0 = std::max(p_cmd.x, 0);
		const int32_t y0 = std::max(p_cmd.y, 0);
		const int32_t x1 = std::min(saturating_add_i32(p_cmd.x, p_cmd.w), (int32_t)width);
		const int32_t y1 = std::min(saturating_add_i32(p_cmd.y, p_cmd.h), (int32_t)height);
		if (x0 >= x1 || y0 >= y1) {
			return;
		}
		p_cmd.clip_x = x0;
		p_cmd.clip_y = y0;
		p_cmd.clip_w = x1 - x0;
		p_cmd.clip_h = y1 - y0;
		commands.push_back(p_cmd);
	}

	int64_t width;
	int64_t height;
	CommandList commands;
};

template <typename TDraw>
static void draw_seamless(int64_t p_canvas_w, int64_t p_canvas_h, int64_t p_x, int64_t p_y,
		int64_t p_w, int64_t p_h, bool p_seamless, TDraw p_draw) {
	if (!p_seamless) {
		p_draw(p_x, p_y, p_w, p_h);
		return;
	}
	while (p_x + p_w > p_canvas_w) {
		p_x -= p_canvas_w;
	}
	while (p_y + p_h > p_canvas_h) {
		p_y -= p_canvas_h;
	}
	for (int64_t ox = 0; p_x + ox <= p_canvas_w; ox += p_canvas_w) {
		for (int64_t oy = 0; p_y + oy <= p_canvas_h; oy += p_canvas_h) {
			p_draw(p_x + ox, p_y + oy, p_w, p_h);
		}
	}
}

static int64_t scaled_side(Rng &p_rng, double p_reference, double p_min_divisor, double p_max_divisor, double p_scale) {
	const int64_t drawn = p_rng.integer(round_half_up(p_reference / p_min_divisor), round_half_up(p_reference / p_max_divisor));
	return round_half_up((double)drawn * p_scale);
}

static void draw_rect(Builder &b, Rng &g, const Params &p, uint32_t p_mode) {
	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t gray = g.integer(p.rect_brightness.min, p.rect_brightness.max);
	const int64_t alpha = g.integer(p.rect_alpha.min, p.rect_alpha.max);

	const double scale = (double)p.rect_scale / PERCENT;
	const int64_t rect_w = scaled_side(g, fw, RECT_SIDE_MIN_DIVISOR, RECT_SIDE_MAX_DIVISOR, scale);
	const int64_t rect_h = scaled_side(g, fw, RECT_SIDE_MIN_DIVISOR, RECT_SIDE_MAX_DIVISOR, scale);

	const bool seamless = p.seamless;
	const int64_t x_min = seamless ? 0 : round_half_up(-(double)rect_w / 2.0);
	const int64_t x_max = seamless ? round_half_up(fw) : round_half_up(fw - (double)rect_w / 2.0);
	const int64_t y_min = seamless ? 0 : round_half_up(-(double)rect_h / 2.0);
	const int64_t y_max = seamless ? round_half_up(fh) : round_half_up(fh - (double)rect_h / 2.0);
	const int64_t x = g.integer(x_min, x_max);
	const int64_t y = g.integer(y_min, y_max);

	draw_seamless(b.canvas_width(), b.canvas_height(), x, y, rect_w, rect_h, seamless,
			[&](int64_t dx, int64_t dy, int64_t dw, int64_t dh) { b.push_fill(dx, dy, dw, dh, gray, alpha, p_mode); });
}

static void draw_grid(Builder &b, Rng &g, const Params &p, uint32_t p_mode) {
	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t gray = g.integer(p.grid_brightness.min, p.grid_brightness.max);
	const int64_t alpha = g.integer(p.grid_alpha.min, p.grid_alpha.max);

	const bool seamless = p.seamless;
	const int64_t x0 = g.integer(origin_min(seamless, fw), round_half_up(fw));
	const int64_t y0 = g.integer(origin_min(seamless, fh), round_half_up(fh));
	const int64_t xn = g.integer(p.grid_amount.min, p.grid_amount.max);
	const int64_t yn = g.integer(p.grid_amount.min, p.grid_amount.max);
	const double scale = (double)p.grid_scale / PERCENT;
	const double gap = (double)p.grid_gap / PERCENT;
	const int64_t size = scaled_side(g, fw, CELL_SIDE_MIN_DIVISOR, CELL_SIDE_MAX_DIVISOR, scale);
	const int64_t step = size + round_half_up((double)size * gap);

	int64_t x = x0;
	for (int64_t col = 0; col < std::max<int64_t>(xn, 0); col++) {
		int64_t y = y0;
		for (int64_t row = 0; row < std::max<int64_t>(yn, 0); row++) {
			draw_seamless(b.canvas_width(), b.canvas_height(), x, y, size, size, seamless,
					[&](int64_t dx, int64_t dy, int64_t dw, int64_t dh) { b.push_fill(dx, dy, dw, dh, gray, alpha, p_mode); });
			y += step;
		}
		x += step;
	}
}

static void draw_cols(Builder &b, Rng &g, const Params &p, uint32_t p_mode) {
	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t gray = g.integer(p.cols_brightness.min, p.cols_brightness.max);
	const int64_t alpha = g.integer(p.cols_alpha.min, p.cols_alpha.max);

	const bool seamless = p.seamless;
	const int64_t x0 = g.integer(origin_min(seamless, fw), round_half_up(fw));
	const int64_t y0 = g.integer(origin_min(seamless, fh), round_half_up(fh));
	const int64_t xn = g.integer(p.cols_amount.min, p.cols_amount.max);
	const double scale = (double)p.cols_scale / PERCENT;
	const double gap = (double)p.cols_gap / PERCENT;
	const int64_t size_w = scaled_side(g, fw, CELL_SIDE_MIN_DIVISOR, CELL_SIDE_MAX_DIVISOR, scale);
	const int64_t size_h = round_half_up((double)size_w * (double)g.integer(1, ELONGATION_MAX));
	const int64_t step = size_w + round_half_up((double)size_w * gap);

	int64_t x = x0;
	for (int64_t col = 0; col < std::max<int64_t>(xn, 0); col++) {
		draw_seamless(b.canvas_width(), b.canvas_height(), x, y0, size_w, size_h, seamless,
				[&](int64_t dx, int64_t dy, int64_t dw, int64_t dh) { b.push_fill(dx, dy, dw, dh, gray, alpha, p_mode); });
		x += step;
	}
}

static void draw_rows(Builder &b, Rng &g, const Params &p, uint32_t p_mode) {
	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t gray = g.integer(p.rows_brightness.min, p.rows_brightness.max);
	const int64_t alpha = g.integer(p.rows_alpha.min, p.rows_alpha.max);

	const bool seamless = p.seamless;
	const int64_t x0 = g.integer(origin_min(seamless, fw), round_half_up(fw));
	const int64_t y0 = g.integer(origin_min(seamless, fh), round_half_up(fh));
	const int64_t yn = g.integer(p.rows_amount.min, p.rows_amount.max);
	const double scale = (double)p.rows_scale / PERCENT;
	const double gap = (double)p.rows_gap / PERCENT;
	const int64_t size_h = scaled_side(g, fw, CELL_SIDE_MIN_DIVISOR, CELL_SIDE_MAX_DIVISOR, scale);
	const int64_t size_w = round_half_up((double)size_h * (double)g.integer(1, ELONGATION_MAX));
	const int64_t step = size_h + round_half_up((double)size_h * gap);

	int64_t y = y0;
	for (int64_t row = 0; row < std::max<int64_t>(yn, 0); row++) {
		draw_seamless(b.canvas_width(), b.canvas_height(), x0, y, size_w, size_h, seamless,
				[&](int64_t dx, int64_t dy, int64_t dw, int64_t dh) { b.push_fill(dx, dy, dw, dh, gray, alpha, p_mode); });
		y += step;
	}
}

static void draw_lines(Builder &b, Rng &g, const Params &p, uint32_t p_mode) {
	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t gray = g.integer(p.lines_brightness.min, p.lines_brightness.max);
	const int64_t alpha = g.integer(p.lines_alpha.min, p.lines_alpha.max);

	if (g.boolean()) {
		const int64_t y = g.integer(origin_min(false, fh), round_half_up(fh));
		const int64_t thickness = round_half_up((double)g.integer(p.lines_width.min, p.lines_width.max) * (fh / LINE_THICKNESS_REFERENCE_SIDE));
		b.push_fill(0, y, b.canvas_width(), thickness, gray, alpha, p_mode);
		return;
	}
	const int64_t x = g.integer(origin_min(false, fw), round_half_up(fw));
	const int64_t thickness = round_half_up((double)g.integer(p.lines_width.min, p.lines_width.max) * (fw / LINE_THICKNESS_REFERENCE_SIDE));
	b.push_fill(x, 0, thickness, b.canvas_height(), gray, alpha, p_mode);
}

static void draw_sprite(Builder &b, Rng &g, const Params &p, uint32_t p_sprite_count, uint32_t p_mode) {
	if (p_sprite_count == 0) {
		return;
	}
	const uint32_t index = (uint32_t)g.integer(0, (int64_t)p_sprite_count - 1);

	const double fw = (double)b.canvas_width();
	const double fh = (double)b.canvas_height();
	const int64_t size = g.integer(round_half_up(fw / SPRITE_SIDE_MIN_DIVISOR), round_half_up(fw / SPRITE_SIDE_MAX_DIVISOR));

	const bool seamless = p.seamless;
	const int64_t x = g.integer(origin_min(seamless, fw), round_half_up(fw));
	const int64_t y = g.integer(origin_min(seamless, fh), round_half_up(fh));
	const int64_t angle = g.integer(0, MAX_QUARTER_TURNS) * QUARTER_TURN_DEGREES;
	const int64_t rotation = p.sprites_rotation_enabled ? angle : 0;

	draw_seamless(b.canvas_width(), b.canvas_height(), x, y, size, size, seamless,
			[&](int64_t dx, int64_t dy, int64_t, int64_t) { b.push_sprite(dx, dy, size, index, rotation, p_mode); });
}

CommandList build_command_list(const Params &p_params, uint32_t p_width, uint32_t p_height, uint64_t p_seed) {
	Builder b((int64_t)p_width, (int64_t)p_height);
	Rng g(p_seed);

	b.push_fill(0, 0, (int64_t)p_width, (int64_t)p_height,
			clamp_i64(p_params.background_brightness, 0, GRAY_MAX), ALPHA_OPAQUE, MODE_SOURCE_OVER);

	const uint32_t sprites = p_params.sprites_enabled ? sprite_count(p_params.sprite_pack_mask) : 0;
	const std::vector<uint32_t> modes = composition_mode_codes(p_params.composition_mode_mask);

	uint32_t mode = MODE_SOURCE_OVER;
	for (int64_t i = 0; i < std::max<int64_t>(p_params.iterations, 0); i++) {
		mode = modes[(size_t)g.integer(0, (int64_t)modes.size() - 1)];
		switch (g.integer(0, LAYER_KIND_COUNT - 1)) {
			case 0:
				if (p_params.rect_enabled) {
					draw_rect(b, g, p_params, mode);
				}
				break;
			case 1:
				if (p_params.grid_enabled) {
					draw_grid(b, g, p_params, mode);
				}
				break;
			case 2:
				if (p_params.cols_enabled) {
					draw_cols(b, g, p_params, mode);
				}
				break;
			case 3:
				if (p_params.rows_enabled) {
					draw_rows(b, g, p_params, mode);
				}
				break;
			case 4:
				if (p_params.lines_enabled) {
					draw_lines(b, g, p_params, mode);
				}
				break;
			default:
				if (p_params.sprites_enabled) {
					draw_sprite(b, g, p_params, sprites, mode);
				}
				break;
		}
	}
	return b.take();
}

} // namespace cppdx
