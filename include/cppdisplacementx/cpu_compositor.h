#ifndef CPPDX_CPU_COMPOSITOR_H
#define CPPDX_CPU_COMPOSITOR_H

#include "cppdisplacementx/blend.h"
#include "cppdisplacementx/canvas.h"
#include "cppdisplacementx/draw_command.h"

namespace cppdx {

// Applies the command list in submission order, producing bytes identical to
// the GPU compositor. p_threads == 0 asks for hardware_concurrency.
void composite_cpu(Canvas &r_canvas, const CommandList &p_commands, const AtlasView &p_atlas, uint32_t p_threads);

} // namespace cppdx

#endif // CPPDX_CPU_COMPOSITOR_H
