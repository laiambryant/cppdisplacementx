#include "cppdisplacementx/params.h"

namespace cppdx {

static const char *COMPOSITION_NAMES[COMPOSITION_MODE_COUNT] = {
	"color-burn", "color-dodge", "darken", "difference", "exclusion",
	"hard-light", "lighten", "lighter", "luminosity", "multiply",
	"overlay", "screen", "soft-light", "source-atop", "source-over", "xor"
};

static const char *SPRITE_PACK_NAMES[SPRITE_PACK_COUNT] = {
	"classic", "bigdata", "aggromaxx", "crappack"
};

static const uint32_t SPRITE_PACK_LENGTHS[SPRITE_PACK_COUNT] = { 17, 5, 12, 27 };

const char *composition_mode_name(uint32_t p_code) {
	return p_code < COMPOSITION_MODE_COUNT ? COMPOSITION_NAMES[p_code] : COMPOSITION_NAMES[MODE_SOURCE_OVER];
}

const char *sprite_pack_name(uint32_t p_pack) {
	return p_pack < SPRITE_PACK_COUNT ? SPRITE_PACK_NAMES[p_pack] : "";
}

uint32_t sprite_pack_length(uint32_t p_pack) {
	return p_pack < SPRITE_PACK_COUNT ? SPRITE_PACK_LENGTHS[p_pack] : 0;
}

std::vector<uint32_t> composition_mode_codes(uint32_t p_mask) {
	std::vector<uint32_t> codes;
	for (uint32_t i = 0; i < COMPOSITION_MODE_COUNT; i++) {
		if (p_mask & (1u << i)) {
			codes.push_back(i);
		}
	}
	if (codes.empty()) {
		codes.push_back(MODE_SOURCE_OVER);
	}
	return codes;
}

std::vector<uint32_t> selected_sprite_packs(uint32_t p_mask) {
	std::vector<uint32_t> packs;
	for (uint32_t i = 0; i < SPRITE_PACK_COUNT; i++) {
		if (p_mask & (1u << i)) {
			packs.push_back(i);
		}
	}
	return packs;
}

uint32_t sprite_count(uint32_t p_mask) {
	uint32_t total = 0;
	for (uint32_t pack : selected_sprite_packs(p_mask)) {
		total += sprite_pack_length(pack);
	}
	return total;
}

} // namespace cppdx
