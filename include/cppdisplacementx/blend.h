#ifndef CPPDX_BLEND_H
#define CPPDX_BLEND_H

#include "cppdisplacementx/draw_command.h"
#include "cppdisplacementx/params.h"

#include <cstdint>

namespace cppdx {

// Integer-only mirror of composite.glsl. Every operation here has a line-for-line
// counterpart in the shader: the two must agree bit for bit, which is what lets
// the GPU and CPU backends be selected interchangeably for the same seed.
// Channels and alphas live on 0..255; percentages convert via
// sa = min(255, (pct * 255 + 50) / 100).

struct Rgba {
	uint32_t r = 0;
	uint32_t g = 0;
	uint32_t b = 0;
	uint32_t a = 0;
};

// Flat sprite atlas: pixels packed r | g<<8 | b<<16 | a<<24, meta holding an
// [offset, side] pair per sprite.
struct AtlasView {
	const uint32_t *pixels = nullptr;
	const uint32_t *meta = nullptr;
};

inline uint32_t div255(uint32_t p_value) {
	return (p_value + 127u) / 255u;
}

inline uint32_t div_round(uint32_t p_num, uint32_t p_den) {
	return (p_num + p_den / 2u) / p_den;
}

inline int32_t div_round_i(int32_t p_num, int32_t p_den) {
	if (p_num >= 0) {
		return (p_num + p_den / 2) / p_den;
	}
	return -((-p_num + p_den / 2) / p_den);
}

inline uint32_t clamp255(int32_t p_value) {
	return (uint32_t)(p_value < 0 ? 0 : (p_value > 255 ? 255 : p_value));
}

inline int32_t floor_shift_8(int32_t p_value) {
	return p_value >= 0 ? (p_value >> 8) : -((-p_value + 255) >> 8);
}

inline uint32_t isqrt(uint32_t p_value) {
	uint32_t x = p_value;
	uint32_t result = 0;
	uint32_t bit = 1u << 14u;
	while (bit != 0u) {
		const uint32_t t = result + bit;
		if (x >= t) {
			x -= t;
			result = (result >> 1u) + bit;
		} else {
			result = result >> 1u;
		}
		bit >>= 2u;
	}
	return result;
}

inline Rgba unpack_px(uint32_t p_value) {
	return Rgba{ p_value & 255u, (p_value >> 8u) & 255u, (p_value >> 16u) & 255u, (p_value >> 24u) & 255u };
}

inline uint32_t pack_px(const Rgba &p_color) {
	return p_color.r | (p_color.g << 8u) | (p_color.b << 16u) | (p_color.a << 24u);
}

inline uint32_t soft_light(uint32_t p_cb, uint32_t p_cs) {
	if (2u * p_cs <= 255u) {
		const uint32_t sub = div_round((255u - 2u * p_cs) * p_cb * (255u - p_cb), 65025u);
		const int32_t v = (int32_t)p_cb - (int32_t)sub;
		return (uint32_t)(v > 0 ? v : 0);
	}
	uint32_t d;
	if (4u * p_cb <= 255u) {
		const int32_t poly = (16 * (int32_t)p_cb - 3060) * (int32_t)p_cb + 260100;
		d = div_round((uint32_t)poly * p_cb, 65025u);
	} else {
		d = isqrt(p_cb * 255u);
	}
	const int32_t num = (2 * (int32_t)p_cs - 255) * ((int32_t)d - (int32_t)p_cb);
	return clamp255((int32_t)p_cb + div_round_i(num, 255));
}

inline uint32_t sep_blend(uint32_t p_cb, uint32_t p_cs, uint32_t p_mode) {
	switch (p_mode) {
		case MODE_MULTIPLY:
			return div255(p_cb * p_cs);
		case MODE_SCREEN:
			return p_cb + p_cs - div255(p_cb * p_cs);
		case MODE_OVERLAY:
			if (2u * p_cb <= 255u) {
				return div255(2u * p_cs * p_cb);
			}
			return 255u - div255(2u * (255u - p_cs) * (255u - p_cb));
		case MODE_DARKEN:
			return p_cb < p_cs ? p_cb : p_cs;
		case MODE_LIGHTEN:
			return p_cb > p_cs ? p_cb : p_cs;
		case MODE_COLOR_DODGE: {
			if (p_cb == 0u) {
				return 0u;
			}
			if (p_cs == 255u) {
				return 255u;
			}
			const uint32_t v = div_round(p_cb * 255u, 255u - p_cs);
			return v < 255u ? v : 255u;
		}
		case MODE_COLOR_BURN: {
			if (p_cb == 255u) {
				return 255u;
			}
			if (p_cs == 0u) {
				return 0u;
			}
			const uint32_t v = div_round((255u - p_cb) * 255u, p_cs);
			return 255u - (v < 255u ? v : 255u);
		}
		case MODE_HARD_LIGHT:
			if (2u * p_cs <= 255u) {
				return div255(2u * p_cs * p_cb);
			}
			return 255u - div255(2u * (255u - p_cs) * (255u - p_cb));
		case MODE_SOFT_LIGHT:
			return soft_light(p_cb, p_cs);
		case MODE_DIFFERENCE: {
			const int32_t d = (int32_t)p_cb - (int32_t)p_cs;
			return (uint32_t)(d < 0 ? -d : d);
		}
		case MODE_EXCLUSION:
			return clamp255((int32_t)p_cb + (int32_t)p_cs - (int32_t)div255(2u * p_cb * p_cs));
		default:
			return p_cs;
	}
}

inline uint32_t lum255(uint32_t p_r, uint32_t p_g, uint32_t p_b) {
	return (77u * p_r + 150u * p_g + 29u * p_b + 128u) >> 8u;
}

inline void set_lum(uint32_t p_cb_r, uint32_t p_cb_g, uint32_t p_cb_b, uint32_t p_lum, Rgba &r_out) {
	const int32_t d = (int32_t)p_lum - (int32_t)lum255(p_cb_r, p_cb_g, p_cb_b);
	int32_t r = (int32_t)p_cb_r + d;
	int32_t g = (int32_t)p_cb_g + d;
	int32_t b = (int32_t)p_cb_b + d;
	const int32_t lt = (int32_t)p_lum;
	const int32_t n = r < g ? (r < b ? r : b) : (g < b ? g : b);
	const int32_t x = r > g ? (r > b ? r : b) : (g > b ? g : b);
	if (n < 0) {
		r = lt + div_round_i((r - lt) * lt, lt - n);
		g = lt + div_round_i((g - lt) * lt, lt - n);
		b = lt + div_round_i((b - lt) * lt, lt - n);
	}
	if (x > 255) {
		r = lt + div_round_i((r - lt) * (255 - lt), x - lt);
		g = lt + div_round_i((g - lt) * (255 - lt), x - lt);
		b = lt + div_round_i((b - lt) * (255 - lt), x - lt);
	}
	r_out.r = clamp255(r);
	r_out.g = clamp255(g);
	r_out.b = clamp255(b);
}

inline bool is_porter_duff(uint32_t p_mode) {
	return p_mode == MODE_SOURCE_OVER || p_mode == MODE_SOURCE_ATOP || p_mode == MODE_XOR || p_mode == MODE_LIGHTER;
}

inline Rgba composite(const Rgba &p_dst, uint32_t p_cs_r, uint32_t p_cs_g, uint32_t p_cs_b, uint32_t p_sa, uint32_t p_mode) {
	if (p_sa == 0u) {
		return p_dst;
	}
	const uint32_t ab = p_dst.a;

	uint32_t fa = 255u;
	uint32_t fb = 255u - p_sa;
	if (p_mode == MODE_SOURCE_ATOP) {
		fa = ab;
	} else if (p_mode == MODE_XOR) {
		fa = 255u - ab;
	} else if (p_mode == MODE_LIGHTER) {
		fb = 255u;
	}

	Rgba bl;
	if (is_porter_duff(p_mode)) {
		bl.r = p_cs_r;
		bl.g = p_cs_g;
		bl.b = p_cs_b;
	} else if (p_mode == MODE_LUMINOSITY) {
		set_lum(p_dst.r, p_dst.g, p_dst.b, lum255(p_cs_r, p_cs_g, p_cs_b), bl);
	} else {
		bl.r = sep_blend(p_dst.r, p_cs_r, p_mode);
		bl.g = sep_blend(p_dst.g, p_cs_g, p_mode);
		bl.b = sep_blend(p_dst.b, p_cs_b, p_mode);
	}

	const uint32_t csp_r = div255((255u - ab) * p_cs_r + ab * bl.r);
	const uint32_t csp_g = div255((255u - ab) * p_cs_g + ab * bl.g);
	const uint32_t csp_b = div255((255u - ab) * p_cs_b + ab * bl.b);

	const uint32_t wa = p_sa * fa;
	const uint32_t wb = ab * fb;
	const uint32_t denom = wa + wb;
	if (denom == 0u) {
		return Rgba{ 0u, 0u, 0u, 0u };
	}
	const uint32_t alpha = div255(denom);
	return Rgba{
		div_round(wa * csp_r + wb * p_dst.r, denom),
		div_round(wa * csp_g + wb * p_dst.g, denom),
		div_round(wa * csp_b + wb * p_dst.b, denom),
		alpha < 255u ? alpha : 255u
	};
}

inline Rgba premul(const Rgba &p_color) {
	return Rgba{ div255(p_color.r * p_color.a), div255(p_color.g * p_color.a), div255(p_color.b * p_color.a), p_color.a };
}

inline Rgba sample_sprite(const AtlasView &p_atlas, uint32_t p_sprite, int32_t p_size, int32_t p_ox, int32_t p_oy, uint32_t p_rotation) {
	int32_t sx = p_ox;
	int32_t sy = p_oy;
	if (p_rotation == 90u) {
		sx = p_oy;
		sy = p_size - 1 - p_ox;
	} else if (p_rotation == 180u) {
		sx = p_size - 1 - p_ox;
		sy = p_size - 1 - p_oy;
	} else if (p_rotation == 270u) {
		sx = p_size - 1 - p_oy;
		sy = p_ox;
	}
	const uint32_t base = p_atlas.meta[p_sprite * 2];
	const int32_t src_n = (int32_t)p_atlas.meta[p_sprite * 2 + 1];

	const int32_t sxq = (int32_t)((uint32_t)(sx * 2 + 1) * (uint32_t)src_n * 256u / (uint32_t)(p_size * 2)) - 128;
	const int32_t syq = (int32_t)((uint32_t)(sy * 2 + 1) * (uint32_t)src_n * 256u / (uint32_t)(p_size * 2)) - 128;
	int32_t x0 = floor_shift_8(sxq);
	int32_t y0 = floor_shift_8(syq);
	const uint32_t fx = (uint32_t)sxq & 255u;
	const uint32_t fy = (uint32_t)syq & 255u;
	int32_t x1 = x0 + 1;
	int32_t y1 = y0 + 1;
	x0 = x0 < 0 ? 0 : (x0 > src_n - 1 ? src_n - 1 : x0);
	x1 = x1 < 0 ? 0 : (x1 > src_n - 1 ? src_n - 1 : x1);
	y0 = y0 < 0 ? 0 : (y0 > src_n - 1 ? src_n - 1 : y0);
	y1 = y1 < 0 ? 0 : (y1 > src_n - 1 ? src_n - 1 : y1);

	const Rgba p00 = premul(unpack_px(p_atlas.pixels[base + (uint32_t)(y0 * src_n + x0)]));
	const Rgba p10 = premul(unpack_px(p_atlas.pixels[base + (uint32_t)(y0 * src_n + x1)]));
	const Rgba p01 = premul(unpack_px(p_atlas.pixels[base + (uint32_t)(y1 * src_n + x0)]));
	const Rgba p11 = premul(unpack_px(p_atlas.pixels[base + (uint32_t)(y1 * src_n + x1)]));

	const uint32_t fx1 = 256u - fx;
	const uint32_t fy1 = 256u - fy;
	const uint32_t r = ((p00.r * fx1 + p10.r * fx) * fy1 + (p01.r * fx1 + p11.r * fx) * fy + 32768u) >> 16u;
	const uint32_t g = ((p00.g * fx1 + p10.g * fx) * fy1 + (p01.g * fx1 + p11.g * fx) * fy + 32768u) >> 16u;
	const uint32_t b = ((p00.b * fx1 + p10.b * fx) * fy1 + (p01.b * fx1 + p11.b * fx) * fy + 32768u) >> 16u;
	const uint32_t a = ((p00.a * fx1 + p10.a * fx) * fy1 + (p01.a * fx1 + p11.a * fx) * fy + 32768u) >> 16u;

	if (a == 0u) {
		return Rgba{ 0u, 0u, 0u, 0u };
	}
	const uint32_t ur = div_round(r * 255u, a);
	const uint32_t ug = div_round(g * 255u, a);
	const uint32_t ub = div_round(b * 255u, a);
	return Rgba{ ur < 255u ? ur : 255u, ug < 255u ? ug : 255u, ub < 255u ? ub : 255u, a };
}

inline uint32_t alpha_from_percent(uint32_t p_percent) {
	const uint32_t sa = (p_percent * 255u + 50u) / 100u;
	return sa < 255u ? sa : 255u;
}

inline Rgba apply_command(const DrawCommand &p_cmd, const AtlasView &p_atlas, int32_t p_px, int32_t p_py, const Rgba &p_dst) {
	if (p_cmd.kind == CMD_FILL_RECT) {
		return composite(p_dst, p_cmd.gray, p_cmd.gray, p_cmd.gray, alpha_from_percent(p_cmd.alpha), p_cmd.mode);
	}
	const Rgba s = sample_sprite(p_atlas, p_cmd.sprite, p_cmd.w, p_px - p_cmd.x, p_py - p_cmd.y, p_cmd.rot);
	return composite(p_dst, s.r, s.g, s.b, s.a, p_cmd.mode);
}

} // namespace cppdx

#endif // CPPDX_BLEND_H
