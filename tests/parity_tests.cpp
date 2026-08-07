#include "cppdisplacementx/engine.h"
#include "cppdisplacementx/rng.h"
#include "cppdisplacementx/simd_row_blend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace cppdx;

static int failures = 0;

static void check(bool p_condition, const std::string &p_what) {
	if (!p_condition) {
		std::printf("FAIL %s\n", p_what.c_str());
		failures++;
	}
}

static Params all_layers_params() {
	Params p;
	p.composition_mode_mask = 0xFFFF;
	return p;
}

static void rng_stream_is_frozen() {
	Rng a(1);
	Rng b(1);
	for (int i = 0; i < 8; i++) {
		check(a.next_u32() == b.next_u32(), "rng repeats for the same seed");
	}
	Rng c(42);
	bool saw_min = false;
	bool saw_max = false;
	for (int i = 0; i < 10000; i++) {
		const int64_t v = c.integer(-3, 3);
		check(v >= -3 && v <= 3, "rng integer stays in range");
		saw_min |= v == -3;
		saw_max |= v == 3;
	}
	check(saw_min && saw_max, "rng integer is inclusive on both ends");
	check(c.integer(7, 7) == 7, "rng integer collapses an empty range");
}

static void command_list_is_deterministic() {
	const Params p = all_layers_params();
	const CommandList a = build_command_list(p, 512, 512, 424242);
	const CommandList b = build_command_list(p, 512, 512, 424242);
	check(a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(DrawCommand)) == 0,
			"command list repeats for the same seed");
	check(a.size() > 1, "command list is not empty");

	const CommandList other = build_command_list(p, 512, 512, 1);
	check(other.size() != a.size() || std::memcmp(a.data(), other.data(), a.size() * sizeof(DrawCommand)) != 0,
			"command list differs for another seed");
}

static void commands_are_clipped_to_the_canvas() {
	const CommandList cmds = build_command_list(all_layers_params(), 300, 300, 7);
	for (const DrawCommand &cmd : cmds) {
		check(cmd.clip_x >= 0 && cmd.clip_y >= 0, "clip origin is on canvas");
		check(cmd.clip_x + cmd.clip_w <= 300 && cmd.clip_y + cmd.clip_h <= 300, "clip rect ends on canvas");
		check(cmd.clip_w > 0 && cmd.clip_h > 0, "clip rect is not degenerate");
	}
}

static void background_is_the_first_command() {
	const CommandList cmds = build_command_list(Params(), 128, 128, 5);
	check(!cmds.empty(), "background command exists");
	check(cmds[0].kind == CMD_FILL_RECT, "background is a fill");
	check(cmds[0].clip_w == 128 && cmds[0].clip_h == 128, "background covers the canvas");
	check(cmds[0].gray == 32, "background uses the default brightness");
	check(cmds[0].mode == MODE_SOURCE_OVER, "background composites source-over");
}

static uint32_t scalar_blend_pixel(uint32_t p_dst, uint32_t p_gray, uint32_t p_alpha_percent, uint32_t p_mode) {
	DrawCommand cmd = {};
	cmd.kind = CMD_FILL_RECT;
	cmd.mode = p_mode;
	cmd.gray = p_gray;
	cmd.alpha = p_alpha_percent;
	const AtlasView atlas{ nullptr, nullptr };
	return pack_px(apply_command(cmd, atlas, 0, 0, unpack_px(p_dst)));
}

static void simd_rows_match_the_scalar_path() {
	const uint32_t modes[] = { MODE_SOURCE_OVER, MODE_SOURCE_ATOP, MODE_MULTIPLY, MODE_SCREEN,
		MODE_DARKEN, MODE_LIGHTEN, MODE_DIFFERENCE, MODE_OVERLAY, MODE_HARD_LIGHT };
	Rng noise(99);
	for (uint32_t mode : modes) {
		for (uint32_t gray = 0; gray < 256u; gray += 17u) {
			for (uint32_t percent = 0; percent <= 100u; percent += 7u) {
				std::vector<uint32_t> row(64);
				for (uint32_t &pixel : row) {
					pixel = pack_px(Rgba{ (uint32_t)noise.integer(0, 255), (uint32_t)noise.integer(0, 255),
							(uint32_t)noise.integer(0, 255), 255u });
				}
				std::vector<uint32_t> expected(row.size());
				for (size_t i = 0; i < row.size(); i++) {
					expected[i] = scalar_blend_pixel(row[i], gray, percent, mode);
				}
				const uint32_t done = blend_row_opaque(row.data(), (uint32_t)row.size(), gray,
						alpha_from_percent(percent), mode);
				for (uint32_t i = 0; i < done; i++) {
					check(row[i] == expected[i], "simd row blend matches the scalar path");
				}
			}
		}
	}
}

static void bins_cover_every_command() {
	const CommandList cmds = build_command_list(all_layers_params(), 256, 256, 11);
	const TileBins bins = bin_commands(cmds, 256, 256);
	check(bins.tile_cols == 8 && bins.tile_rows == 8, "tile grid matches the canvas");
	size_t total = 0;
	for (size_t tile = 0; tile < bins.ranges.size() / 2; tile++) {
		total += bins.ranges[tile * 2 + 1];
	}
	check(total == bins.indices.size(), "bin ranges span the index array");
}

static void post_matches_the_contract() {
	const Palette palette = build_palette(default_gradient());
	check(palette.r[0] == 0 && palette.g[0] == 255 && palette.b[0] == 255, "palette starts on the first stop");
	check(palette.r[255] == 255 && palette.g[255] == 229 && palette.b[255] == 0, "palette ends on the last stop");

	Canvas flat(2, 2);
	for (uint32_t &pixel : flat.pixels) {
		pixel = pack_px(Rgba{ 80, 80, 80, 255 });
	}
	apply_normal(flat);
	for (uint32_t pixel : flat.pixels) {
		check(pixel == pack_px(Rgba{ 127, 127, 255, 255 }), "normal of a flat field is neutral");
	}

	Canvas colours(1, 1);
	colours.pixels[0] = pack_px(Rgba{ 0, 10, 20, 128 });
	apply_invert(colours);
	check(colours.pixels[0] == pack_px(Rgba{ 255, 245, 235, 128 }), "invert flips rgb and keeps alpha");
}

static void cpu_render_is_deterministic() {
	SpriteAtlas atlas;
	atlas.make_empty_if_unused();
	const Params p = all_layers_params();
	const Canvas a = render_field_cpu(p, 128, 128, 2024, atlas, 1);
	const Canvas b = render_field_cpu(p, 128, 128, 2024, atlas, 4);
	check(a.pixels == b.pixels, "cpu render is thread-count independent");
}

int main() {
	rng_stream_is_frozen();
	command_list_is_deterministic();
	commands_are_clipped_to_the_canvas();
	background_is_the_first_command();
	simd_rows_match_the_scalar_path();
	bins_cover_every_command();
	post_matches_the_contract();
	cpu_render_is_deterministic();

	if (failures == 0) {
		std::printf("all cppdisplacementx tests passed\n");
		return 0;
	}
	std::printf("%d check(s) failed\n", failures);
	return 1;
}
