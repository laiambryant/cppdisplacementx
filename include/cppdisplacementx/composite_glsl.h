#ifndef CPPDX_COMPOSITE_GLSL_H
#define CPPDX_COMPOSITE_GLSL_H

namespace cppdx {

// Vulkan GLSL compute source for the tiled ordered compositor, ready to hand to
// a host's shader compiler. Its arithmetic is the line-for-line counterpart of
// blend.h: any edit here must be mirrored there or the two backends diverge.
//
// Bindings, set 0: 0 canvas (rw), 1 commands, 2 atlas, 3 sprite meta,
// 4 bin indices, 5 tile ranges. Push constant: canvas_w, canvas_h, tile_cols.
const char *composite_compute_glsl();

} // namespace cppdx

#endif // CPPDX_COMPOSITE_GLSL_H
