#ifndef CPPDX_CANVAS_H
#define CPPDX_CANVAS_H

#include <cstdint>
#include <vector>

namespace cppdx {

// Straight-alpha RGBA8 stored one pixel per word, r | g<<8 | b<<16 | a<<24 —
// the same packing the GPU canvas buffer uses, so a readback is a memcpy.
struct Canvas {
	uint32_t width = 0;
	uint32_t height = 0;
	std::vector<uint32_t> pixels;

	Canvas() = default;
	Canvas(uint32_t p_width, uint32_t p_height) :
			width(p_width), height(p_height), pixels((size_t)p_width * (size_t)p_height, 0u) {}

	size_t pixel_count() const { return (size_t)width * (size_t)height; }
	size_t byte_size() const { return pixel_count() * sizeof(uint32_t); }
};

} // namespace cppdx

#endif // CPPDX_CANVAS_H
