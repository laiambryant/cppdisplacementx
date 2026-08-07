#include "cppdisplacementx/rng.h"

namespace cppdx {

static constexpr uint64_t PCG_MULT = 6364136223846793005ULL;
static constexpr uint64_t SEED_MIX = 0x9e3779b97f4a7c15ULL;
static constexpr uint32_t BOOLEAN_TRUE_MIN = 0x80000000u;

static uint32_t rotate_right_32(uint32_t p_value, uint32_t p_rotation) {
	const uint32_t r = p_rotation & 31u;
	return (p_value >> r) | (p_value << ((32u - r) & 31u));
}

Rng::Rng(uint64_t p_seed) {
	inc = ((p_seed ^ SEED_MIX) << 1) | 1u;
	state = 0;
	step();
	state += p_seed;
	step();
}

void Rng::step() {
	state = state * PCG_MULT + inc;
}

uint32_t Rng::next_u32() {
	const uint64_t old = state;
	step();
	const uint32_t xorshifted = (uint32_t)(((old >> 18) ^ old) >> 27);
	return rotate_right_32(xorshifted, (uint32_t)(old >> 59));
}

int64_t Rng::integer(int64_t p_min, int64_t p_max) {
	if (p_max <= p_min) {
		return p_min;
	}
	const uint64_t span = (uint64_t)(p_max - p_min + 1);
	return p_min + (int64_t)((uint64_t)next_u32() % span);
}

bool Rng::boolean() {
	return next_u32() >= BOOLEAN_TRUE_MIN;
}

} // namespace cppdx
