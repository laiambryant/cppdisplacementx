#ifndef CPPDX_ENGINE_H
#define CPPDX_ENGINE_H

#include "cppdisplacementx/canvas.h"
#include "cppdisplacementx/command_list.h"
#include "cppdisplacementx/cpu_compositor.h"
#include "cppdisplacementx/params.h"
#include "cppdisplacementx/post.h"
#include "cppdisplacementx/sprite_atlas.h"
#include "cppdisplacementx/tile_bins.h"

namespace cppdx {

Canvas render_field_cpu(const Params &p_params, uint32_t p_width, uint32_t p_height, uint64_t p_seed,
		const SpriteAtlas &p_atlas, uint32_t p_threads);

} // namespace cppdx

#endif // CPPDX_ENGINE_H
