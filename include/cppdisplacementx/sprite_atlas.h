#ifndef CPPDX_SPRITE_ATLAS_H
#define CPPDX_SPRITE_ATLAS_H

#include "cppdisplacementx/blend.h"

#include <cstdint>
#include <vector>

namespace cppdx {

// Flat sprite atlas. The library never decodes images: the host adds each
// already-decoded square sprite, in canonical pack order, and the resulting
// buffers upload verbatim to the shader.
class SpriteAtlas {
public:
	void add_sprite(const uint32_t *p_rgba_pixels, uint32_t p_side);
	void make_empty_if_unused();

	AtlasView view() const { return AtlasView{ pixels.data(), meta.data() }; }
	uint32_t sprite_count() const { return (uint32_t)(meta.size() / 2); }
	const std::vector<uint32_t> &pixel_data() const { return pixels; }
	const std::vector<uint32_t> &meta_data() const { return meta; }

private:
	std::vector<uint32_t> pixels;
	std::vector<uint32_t> meta;
};

} // namespace cppdx

#endif // CPPDX_SPRITE_ATLAS_H
