#include "cppdisplacementx/cpu_compositor.h"

#include "cppdisplacementx/simd_row_blend.h"

#include <algorithm>
#include <thread>
#include <vector>

namespace cppdx {

static constexpr int64_t PARALLEL_AREA_THRESHOLD = 1 << 16;

static bool covers_whole_canvas(const DrawCommand &p_cmd, const Canvas &p_canvas) {
	return p_cmd.clip_x == 0 && p_cmd.clip_y == 0 &&
			(uint32_t)p_cmd.clip_w == p_canvas.width && (uint32_t)p_cmd.clip_h == p_canvas.height;
}

static bool leaves_canvas_opaque(const DrawCommand &p_cmd, const Canvas &p_canvas, bool p_was_opaque) {
	if (p_cmd.mode == MODE_XOR) {
		return false;
	}
	if (p_was_opaque) {
		return true;
	}
	return covers_whole_canvas(p_cmd, p_canvas) && p_cmd.kind == CMD_FILL_RECT &&
			alpha_from_percent(p_cmd.alpha) == 255u && p_cmd.mode != MODE_SOURCE_ATOP;
}

static void apply_command_rows(Canvas &r_canvas, const DrawCommand &p_cmd, const AtlasView &p_atlas,
		bool p_opaque_rows, int32_t p_row_begin, int32_t p_row_end) {
	const uint32_t source_alpha = alpha_from_percent(p_cmd.alpha);
	const bool is_fill = p_cmd.kind == CMD_FILL_RECT;
	const bool replaces_row = is_fill && source_alpha == 255u && p_cmd.mode == MODE_SOURCE_OVER;
	const bool vectorised = is_fill && p_opaque_rows && mode_blends_on_opaque_rows(p_cmd.mode);
	const uint32_t opaque_word = pack_px(Rgba{ p_cmd.gray, p_cmd.gray, p_cmd.gray, 255u });
	const uint32_t span = (uint32_t)p_cmd.clip_w;

	for (int32_t y = p_row_begin; y < p_row_end; y++) {
		uint32_t *row = r_canvas.pixels.data() + (size_t)y * (size_t)r_canvas.width + (size_t)p_cmd.clip_x;
		if (replaces_row) {
			std::fill_n(row, span, opaque_word);
			continue;
		}
		uint32_t x = vectorised ? blend_row_opaque(row, span, p_cmd.gray, source_alpha, p_cmd.mode) : 0u;
		for (; x < span; x++) {
			const Rgba dst = unpack_px(row[x]);
			row[x] = pack_px(apply_command(p_cmd, p_atlas, p_cmd.clip_x + (int32_t)x, y, dst));
		}
	}
}

static uint32_t resolve_thread_count(uint32_t p_requested) {
	if (p_requested > 0) {
		return p_requested;
	}
	const unsigned hardware = std::thread::hardware_concurrency();
	return hardware > 0 ? (uint32_t)hardware : 1u;
}

static void apply_command_parallel(Canvas &r_canvas, const DrawCommand &p_cmd, const AtlasView &p_atlas,
		bool p_opaque_rows, uint32_t p_threads) {
	const int64_t area = (int64_t)p_cmd.clip_w * (int64_t)p_cmd.clip_h;
	const uint32_t bands = (area < PARALLEL_AREA_THRESHOLD || p_threads < 2)
			? 1u
			: std::min(p_threads, (uint32_t)p_cmd.clip_h);
	if (bands < 2) {
		apply_command_rows(r_canvas, p_cmd, p_atlas, p_opaque_rows, p_cmd.clip_y, p_cmd.clip_y + p_cmd.clip_h);
		return;
	}

	const int32_t rows_per_band = (p_cmd.clip_h + (int32_t)bands - 1) / (int32_t)bands;
	std::vector<std::thread> workers;
	workers.reserve(bands - 1);
	for (uint32_t band = 1; band < bands; band++) {
		const int32_t begin = p_cmd.clip_y + (int32_t)band * rows_per_band;
		const int32_t end = std::min(begin + rows_per_band, p_cmd.clip_y + p_cmd.clip_h);
		if (begin >= end) {
			break;
		}
		workers.emplace_back([&r_canvas, &p_cmd, &p_atlas, p_opaque_rows, begin, end]() {
			apply_command_rows(r_canvas, p_cmd, p_atlas, p_opaque_rows, begin, end);
		});
	}
	apply_command_rows(r_canvas, p_cmd, p_atlas, p_opaque_rows, p_cmd.clip_y,
			std::min(p_cmd.clip_y + rows_per_band, p_cmd.clip_y + p_cmd.clip_h));
	for (std::thread &worker : workers) {
		worker.join();
	}
}

void composite_cpu(Canvas &r_canvas, const CommandList &p_commands, const AtlasView &p_atlas, uint32_t p_threads) {
	const uint32_t threads = resolve_thread_count(p_threads);
	bool opaque = false;
	for (const DrawCommand &cmd : p_commands) {
		apply_command_parallel(r_canvas, cmd, p_atlas, opaque, threads);
		opaque = leaves_canvas_opaque(cmd, r_canvas, opaque);
	}
}

} // namespace cppdx
