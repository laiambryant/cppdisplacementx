#ifndef CPPDX_COMMAND_LIST_H
#define CPPDX_COMMAND_LIST_H

#include "cppdisplacementx/draw_command.h"
#include "cppdisplacementx/params.h"

namespace cppdx {

// All randomness in the engine lives here. The shaders and the CPU compositor
// consume no RNG, which is half of the cross-backend determinism contract; the
// other half is the integer-only blend math in blend.h and composite.glsl.
CommandList build_command_list(const Params &p_params, uint32_t p_width, uint32_t p_height, uint64_t p_seed);

} // namespace cppdx

#endif // CPPDX_COMMAND_LIST_H
