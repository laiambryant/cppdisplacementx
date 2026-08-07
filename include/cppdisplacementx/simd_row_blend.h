#ifndef CPPDX_SIMD_ROW_BLEND_H
#define CPPDX_SIMD_ROW_BLEND_H

#include "cppdisplacementx/blend.h"

#include <cstdint>

#if defined(__SSE2__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#define CPPDX_HAS_SSE2 1
#include <emmintrin.h>
#else
#define CPPDX_HAS_SSE2 0
#endif

namespace cppdx {

// Vectorised fill-rect blending, valid only where the destination row is known
// opaque and the mode resolves to fa = 255, fb = 255 - sa. Under those
// conditions the compositor collapses to out = div255(sa * blend + (255 - sa) * dst)
// with the alpha byte untouched, and every intermediate stays below 65536 —
// so the 16-bit lanes reproduce the scalar path exactly rather than approximately.

inline bool mode_blends_on_opaque_rows(uint32_t p_mode) {
	switch (p_mode) {
		case MODE_SOURCE_OVER:
		case MODE_SOURCE_ATOP:
		case MODE_MULTIPLY:
		case MODE_SCREEN:
		case MODE_DARKEN:
		case MODE_LIGHTEN:
		case MODE_DIFFERENCE:
		case MODE_OVERLAY:
		case MODE_HARD_LIGHT:
			return true;
		default:
			return false;
	}
}

#if CPPDX_HAS_SSE2

static inline __m128i div255_epu16(__m128i p_value) {
	const __m128i x = _mm_add_epi16(p_value, _mm_set1_epi16(127));
	return _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(x, _mm_set1_epi16(1)), _mm_srli_epi16(x, 8)), 8);
}

static inline __m128i sep_blend_epu16(__m128i p_cb, __m128i p_cs, uint32_t p_mode) {
	const __m128i full = _mm_set1_epi16(255);
	switch (p_mode) {
		case MODE_MULTIPLY:
			return div255_epu16(_mm_mullo_epi16(p_cb, p_cs));
		case MODE_SCREEN:
			return _mm_sub_epi16(_mm_add_epi16(p_cb, p_cs), div255_epu16(_mm_mullo_epi16(p_cb, p_cs)));
		case MODE_DARKEN:
			return _mm_min_epi16(p_cb, p_cs);
		case MODE_LIGHTEN:
			return _mm_max_epi16(p_cb, p_cs);
		case MODE_DIFFERENCE:
			return _mm_max_epi16(_mm_sub_epi16(p_cb, p_cs), _mm_sub_epi16(p_cs, p_cb));
		case MODE_OVERLAY: {
			const __m128i dark = div255_epu16(_mm_mullo_epi16(_mm_add_epi16(p_cb, p_cb), p_cs));
			const __m128i inv_cb2 = _mm_sub_epi16(_mm_add_epi16(full, full), _mm_add_epi16(p_cb, p_cb));
			const __m128i light = _mm_sub_epi16(full, div255_epu16(_mm_mullo_epi16(inv_cb2, _mm_sub_epi16(full, p_cs))));
			const __m128i is_dark = _mm_cmpgt_epi16(_mm_set1_epi16(256), _mm_add_epi16(p_cb, p_cb));
			return _mm_or_si128(_mm_and_si128(is_dark, dark), _mm_andnot_si128(is_dark, light));
		}
		case MODE_HARD_LIGHT: {
			const uint32_t cs = (uint32_t)((uint16_t)_mm_cvtsi128_si32(p_cs));
			if (2u * cs <= 255u) {
				return div255_epu16(_mm_mullo_epi16(_mm_add_epi16(p_cs, p_cs), p_cb));
			}
			const __m128i inv_cs2 = _mm_sub_epi16(_mm_add_epi16(full, full), _mm_add_epi16(p_cs, p_cs));
			return _mm_sub_epi16(full, div255_epu16(_mm_mullo_epi16(inv_cs2, _mm_sub_epi16(full, p_cb))));
		}
		default:
			return p_cs;
	}
}

static inline __m128i blend_half_epu16(__m128i p_dst, __m128i p_source, __m128i p_alpha, __m128i p_inv_alpha,
		__m128i p_keep_alpha, uint32_t p_mode) {
	const __m128i blended = sep_blend_epu16(p_dst, p_source, p_mode);
	const __m128i mixed = div255_epu16(_mm_add_epi16(_mm_mullo_epi16(p_alpha, blended), _mm_mullo_epi16(p_inv_alpha, p_dst)));
	return _mm_or_si128(_mm_andnot_si128(p_keep_alpha, mixed), _mm_and_si128(p_keep_alpha, p_dst));
}

inline uint32_t blend_row_opaque(uint32_t *p_row, uint32_t p_count, uint32_t p_gray, uint32_t p_source_alpha, uint32_t p_mode) {
	const __m128i zero = _mm_setzero_si128();
	const __m128i source = _mm_set1_epi16((short)p_gray);
	const __m128i alpha = _mm_set1_epi16((short)p_source_alpha);
	const __m128i inv_alpha = _mm_set1_epi16((short)(255u - p_source_alpha));
	const __m128i keep_alpha = _mm_set_epi16(-1, 0, 0, 0, -1, 0, 0, 0);

	uint32_t done = 0;
	for (; done + 4 <= p_count; done += 4) {
		const __m128i packed = _mm_loadu_si128((const __m128i *)(p_row + done));
		const __m128i low = blend_half_epu16(_mm_unpacklo_epi8(packed, zero), source, alpha, inv_alpha, keep_alpha, p_mode);
		const __m128i high = blend_half_epu16(_mm_unpackhi_epi8(packed, zero), source, alpha, inv_alpha, keep_alpha, p_mode);
		_mm_storeu_si128((__m128i *)(p_row + done), _mm_packus_epi16(low, high));
	}
	return done;
}

#else

inline uint32_t blend_row_opaque(uint32_t *, uint32_t, uint32_t, uint32_t, uint32_t) {
	return 0;
}

#endif

} // namespace cppdx

#endif // CPPDX_SIMD_ROW_BLEND_H
