#ifndef CPPDX_DRAW_COMMAND_H
#define CPPDX_DRAW_COMMAND_H

#include <cstdint>
#include <vector>

namespace cppdx {

enum CommandKind : uint32_t {
	CMD_FILL_RECT = 0,
	CMD_SPRITE = 1,
};

// Layout is shared verbatim with the compute shader's Command struct and with
// gpudisplacementx's DrawCommand: 16 x u32, uploaded as raw bytes.
struct DrawCommand {
	uint32_t kind;
	uint32_t mode;
	int32_t x;
	int32_t y;
	int32_t w;
	int32_t h;
	uint32_t gray;
	uint32_t alpha;
	uint32_t sprite;
	uint32_t rot;
	int32_t clip_x;
	int32_t clip_y;
	int32_t clip_w;
	int32_t clip_h;
	uint32_t pad0;
	uint32_t pad1;
};

static_assert(sizeof(DrawCommand) == 64, "DrawCommand must stay 16 x u32 for the shader upload");

using CommandList = std::vector<DrawCommand>;

} // namespace cppdx

#endif // CPPDX_DRAW_COMMAND_H
