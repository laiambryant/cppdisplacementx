#include "cppdisplacementx/composite_glsl.h"

namespace cppdx {

static const char *COMPOSITE_SOURCE = R"GLSL(#version 450

layout(local_size_x = 16, local_size_y = 16, local_size_z = 1) in;

#define MODE_COLOR_BURN 0u
#define MODE_COLOR_DODGE 1u
#define MODE_DARKEN 2u
#define MODE_DIFFERENCE 3u
#define MODE_EXCLUSION 4u
#define MODE_HARD_LIGHT 5u
#define MODE_LIGHTEN 6u
#define MODE_LIGHTER 7u
#define MODE_LUMINOSITY 8u
#define MODE_MULTIPLY 9u
#define MODE_OVERLAY 10u
#define MODE_SCREEN 11u
#define MODE_SOFT_LIGHT 12u
#define MODE_SOURCE_ATOP 13u
#define MODE_SOURCE_OVER 14u
#define MODE_XOR 15u

#define TILE 32u
#define STAGE 64u
#define WG_THREADS 256u

struct Command {
	uint kind;
	uint mode;
	int x;
	int y;
	int w;
	int h;
	uint gray;
	uint alpha;
	uint sprite;
	uint rot;
	int clip_x;
	int clip_y;
	int clip_w;
	int clip_h;
	uint pad0;
	uint pad1;
};

layout(set = 0, binding = 0, std430) restrict buffer CanvasBuffer { uint data[]; } canvas;
layout(set = 0, binding = 1, std430) restrict readonly buffer CommandBuffer { Command data[]; } commands;
layout(set = 0, binding = 2, std430) restrict readonly buffer AtlasBuffer { uint data[]; } atlas;
layout(set = 0, binding = 3, std430) restrict readonly buffer SpriteMetaBuffer { uint data[]; } sprite_meta;
layout(set = 0, binding = 4, std430) restrict readonly buffer BinBuffer { uint data[]; } bins;
layout(set = 0, binding = 5, std430) restrict readonly buffer TileRangeBuffer { uint data[]; } tile_ranges;

layout(push_constant, std430) uniform Globals {
	uint canvas_w;
	uint canvas_h;
	uint tile_cols;
	uint pad0;
} globals;

shared Command staged[STAGE];

uint div255(uint v) {
	return (v + 127u) / 255u;
}

uint div_round(uint num, uint den) {
	return (num + den / 2u) / den;
}

int div_round_i(int num, int den) {
	if (num >= 0) {
		return (num + den / 2) / den;
	}
	return -((-num + den / 2) / den);
}

uint clamp255(int v) {
	return uint(clamp(v, 0, 255));
}

uint isqrt(uint v) {
	uint x = v;
	uint res = 0u;
	uint bit = 1u << 14u;
	while (bit != 0u) {
		uint t = res + bit;
		if (x >= t) {
			x -= t;
			res = (res >> 1u) + bit;
		} else {
			res = res >> 1u;
		}
		bit = bit >> 2u;
	}
	return res;
}

uvec4 unpack_px(uint v) {
	return uvec4(v & 255u, (v >> 8u) & 255u, (v >> 16u) & 255u, (v >> 24u) & 255u);
}

uint pack_px(uvec4 c) {
	return c.r | (c.g << 8u) | (c.b << 16u) | (c.a << 24u);
}

uint soft_light(uint cb, uint cs) {
	if (2u * cs <= 255u) {
		uint sub = div_round((255u - 2u * cs) * cb * (255u - cb), 65025u);
		return uint(max(0, int(cb) - int(sub)));
	}
	uint d;
	if (4u * cb <= 255u) {
		int poly = (16 * int(cb) - 3060) * int(cb) + 260100;
		d = div_round(uint(poly) * cb, 65025u);
	} else {
		d = isqrt(cb * 255u);
	}
	int num = (2 * int(cs) - 255) * (int(d) - int(cb));
	return clamp255(int(cb) + div_round_i(num, 255));
}

uint sep_blend(uint cb, uint cs, uint mode) {
	if (mode == MODE_MULTIPLY) {
		return div255(cb * cs);
	}
	if (mode == MODE_SCREEN) {
		return cb + cs - div255(cb * cs);
	}
	if (mode == MODE_OVERLAY) {
		if (2u * cb <= 255u) {
			return div255(2u * cs * cb);
		}
		return 255u - div255(2u * (255u - cs) * (255u - cb));
	}
	if (mode == MODE_DARKEN) {
		return min(cb, cs);
	}
	if (mode == MODE_LIGHTEN) {
		return max(cb, cs);
	}
	if (mode == MODE_COLOR_DODGE) {
		if (cb == 0u) {
			return 0u;
		}
		if (cs == 255u) {
			return 255u;
		}
		return min(255u, div_round(cb * 255u, 255u - cs));
	}
	if (mode == MODE_COLOR_BURN) {
		if (cb == 255u) {
			return 255u;
		}
		if (cs == 0u) {
			return 0u;
		}
		return 255u - min(255u, div_round((255u - cb) * 255u, cs));
	}
	if (mode == MODE_HARD_LIGHT) {
		if (2u * cs <= 255u) {
			return div255(2u * cs * cb);
		}
		return 255u - div255(2u * (255u - cs) * (255u - cb));
	}
	if (mode == MODE_SOFT_LIGHT) {
		return soft_light(cb, cs);
	}
	if (mode == MODE_DIFFERENCE) {
		return uint(abs(int(cb) - int(cs)));
	}
	if (mode == MODE_EXCLUSION) {
		return uint(clamp(int(cb) + int(cs) - int(div255(2u * cb * cs)), 0, 255));
	}
	return cs;
}

uint lum255(uvec3 c) {
	return (77u * c.r + 150u * c.g + 29u * c.b + 128u) >> 8u;
}

uvec3 set_lum(uvec3 cb, uint l) {
	int d = int(l) - int(lum255(cb));
	int r = int(cb.r) + d;
	int g = int(cb.g) + d;
	int b = int(cb.b) + d;
	int lt = int(l);
	int n = min(r, min(g, b));
	int x = max(r, max(g, b));
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
	return uvec3(clamp255(r), clamp255(g), clamp255(b));
}

bool is_porter_duff(uint mode) {
	return mode == MODE_SOURCE_OVER || mode == MODE_SOURCE_ATOP || mode == MODE_XOR || mode == MODE_LIGHTER;
}

uvec4 composite(uvec4 dst, uvec3 cs, uint sa, uint mode) {
	if (sa == 0u) {
		return dst;
	}
	uint ab = dst.a;

	uint fa = 255u;
	uint fb = 255u - sa;
	if (mode == MODE_SOURCE_ATOP) {
		fa = ab;
	} else if (mode == MODE_XOR) {
		fa = 255u - ab;
	} else if (mode == MODE_LIGHTER) {
		fb = 255u;
	}

	uvec3 bl;
	if (is_porter_duff(mode)) {
		bl = cs;
	} else if (mode == MODE_LUMINOSITY) {
		bl = set_lum(dst.rgb, lum255(cs));
	} else {
		bl = uvec3(sep_blend(dst.r, cs.r, mode), sep_blend(dst.g, cs.g, mode), sep_blend(dst.b, cs.b, mode));
	}

	uvec3 csp = uvec3(
		div255((255u - ab) * cs.r + ab * bl.r),
		div255((255u - ab) * cs.g + ab * bl.g),
		div255((255u - ab) * cs.b + ab * bl.b));

	uint wa = sa * fa;
	uint wb = ab * fb;
	uint denom = wa + wb;
	if (denom == 0u) {
		return uvec4(0u);
	}
	return uvec4(
		div_round(wa * csp.r + wb * dst.r, denom),
		div_round(wa * csp.g + wb * dst.g, denom),
		div_round(wa * csp.b + wb * dst.b, denom),
		min(255u, div255(denom)));
}

uvec4 premul(uvec4 c) {
	return uvec4(div255(c.r * c.a), div255(c.g * c.a), div255(c.b * c.a), c.a);
}

uvec4 sample_sprite(uint sprite, int n, int ox, int oy, uint rot) {
	int sx = ox;
	int sy = oy;
	if (rot == 90u) {
		sx = oy;
		sy = n - 1 - ox;
	} else if (rot == 180u) {
		sx = n - 1 - ox;
		sy = n - 1 - oy;
	} else if (rot == 270u) {
		sx = n - 1 - oy;
		sy = ox;
	}
	uint base = sprite_meta.data[sprite * 2u];
	int src_n = int(sprite_meta.data[sprite * 2u + 1u]);

	int sxq = int(uint(sx * 2 + 1) * uint(src_n) * 256u / uint(n * 2)) - 128;
	int syq = int(uint(sy * 2 + 1) * uint(src_n) * 256u / uint(n * 2)) - 128;
	int x0 = sxq >> 8;
	int y0 = syq >> 8;
	uint fx = uint(sxq & 255);
	uint fy = uint(syq & 255);
	int x1 = x0 + 1;
	int y1 = y0 + 1;
	x0 = clamp(x0, 0, src_n - 1);
	x1 = clamp(x1, 0, src_n - 1);
	y0 = clamp(y0, 0, src_n - 1);
	y1 = clamp(y1, 0, src_n - 1);

	uvec4 p00 = premul(unpack_px(atlas.data[base + uint(y0 * src_n + x0)]));
	uvec4 p10 = premul(unpack_px(atlas.data[base + uint(y0 * src_n + x1)]));
	uvec4 p01 = premul(unpack_px(atlas.data[base + uint(y1 * src_n + x0)]));
	uvec4 p11 = premul(unpack_px(atlas.data[base + uint(y1 * src_n + x1)]));

	uint fx1 = 256u - fx;
	uint fy1 = 256u - fy;
	uvec4 top = p00 * fx1 + p10 * fx;
	uvec4 bot = p01 * fx1 + p11 * fx;
	uvec4 blended = (top * fy1 + bot * fy + uvec4(32768u)) >> uvec4(16u);

	uint a = blended.a;
	if (a == 0u) {
		return uvec4(0u);
	}
	return uvec4(
		min(255u, div_round(blended.r * 255u, a)),
		min(255u, div_round(blended.g * 255u, a)),
		min(255u, div_round(blended.b * 255u, a)),
		a);
}

uvec4 apply_command(Command cmd, int px, int py, uvec4 dst) {
	if (cmd.kind == 0u) {
		uint sa = min(255u, (cmd.alpha * 255u + 50u) / 100u);
		return composite(dst, uvec3(cmd.gray), sa, cmd.mode);
	}
	uvec4 s = sample_sprite(cmd.sprite, cmd.w, px - cmd.x, py - cmd.y, cmd.rot);
	return composite(dst, s.rgb, s.a, cmd.mode);
}

bool covers(Command cmd, int px, int py) {
	return px >= cmd.clip_x && px < cmd.clip_x + cmd.clip_w && py >= cmd.clip_y && py < cmd.clip_y + cmd.clip_h;
}

void main() {
	uvec3 gid = gl_GlobalInvocationID;
	bool in_bounds = gid.x < globals.canvas_w && gid.y < globals.canvas_h;
	int px = int(gid.x);
	int py = int(gid.y);
	uint tile = (gl_WorkGroupID.y / 2u) * globals.tile_cols + (gl_WorkGroupID.x / 2u);
	uint range_start = tile_ranges.data[tile * 2u];
	uint range_count = tile_ranges.data[tile * 2u + 1u];
	uint idx = gid.y * globals.canvas_w + gid.x;
	uint lidx = gl_LocalInvocationIndex;

	uvec4 dst = uvec4(0u);
	if (in_bounds) {
		dst = unpack_px(canvas.data[idx]);
	}

	uint done = 0u;
	while (done < range_count) {
		uint count = min(STAGE, range_count - done);
		for (uint s = lidx; s < count; s += WG_THREADS) {
			staged[s] = commands.data[bins.data[range_start + done + s]];
		}
		memoryBarrierShared();
		barrier();
		if (in_bounds) {
			for (uint i = 0u; i < count; i++) {
				Command cmd = staged[i];
				if (covers(cmd, px, py)) {
					dst = apply_command(cmd, px, py, dst);
				}
			}
		}
		memoryBarrierShared();
		barrier();
		done += count;
	}

	if (in_bounds) {
		canvas.data[idx] = pack_px(dst);
	}
}
)GLSL";

const char *composite_compute_glsl() {
	return COMPOSITE_SOURCE;
}

} // namespace cppdx
