#include "cppdisplacementx/sprite_atlas.h"

namespace cppdx {

void SpriteAtlas::add_sprite(const uint32_t *p_rgba_pixels, uint32_t p_side) {
	if (p_rgba_pixels == nullptr || p_side == 0) {
		return;
	}
	meta.push_back((uint32_t)pixels.size());
	meta.push_back(p_side);
	pixels.insert(pixels.end(), p_rgba_pixels, p_rgba_pixels + (size_t)p_side * (size_t)p_side);
}

void SpriteAtlas::make_empty_if_unused() {
	if (!meta.empty()) {
		return;
	}
	pixels.assign(1, 0u);
	meta.assign({ 0u, 1u });
}

} // namespace cppdx
