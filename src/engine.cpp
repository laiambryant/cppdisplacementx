#include "cppdisplacementx/engine.h"

namespace cppdx {

Canvas render_field_cpu(const Params &p_params, uint32_t p_width, uint32_t p_height, uint64_t p_seed,
		const SpriteAtlas &p_atlas, uint32_t p_threads) {
	Canvas canvas(p_width, p_height);
	const CommandList commands = build_command_list(p_params, p_width, p_height, p_seed);
	composite_cpu(canvas, commands, p_atlas.view(), p_threads);
	return canvas;
}

} // namespace cppdx
