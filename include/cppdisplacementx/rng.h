#ifndef CPPDX_RNG_H
#define CPPDX_RNG_H

#include <cstdint>

namespace cppdx {

// Frozen stream, byte-identical to gpudisplacementx src/engine/rng.rs:
//   inc      = ((seed ^ 0x9e3779b97f4a7c15) << 1) | 1
//   init     = state = 0; step; state += seed; step
//   step     = state * 6364136223846793005 + inc
//   output   = rotr32((((state >> 18) ^ state) >> 27), state >> 59)
//   boolean  = next_u32() >= 0x80000000
//   integer  = min + next_u32() % (max - min + 1), inclusive
// Every seeded city ever generated depends on these constants. They never move.
class Rng {
public:
	explicit Rng(uint64_t p_seed);

	uint32_t next_u32();
	int64_t integer(int64_t p_min, int64_t p_max);
	bool boolean();

private:
	void step();

	uint64_t state = 0;
	uint64_t inc = 0;
};

} // namespace cppdx

#endif // CPPDX_RNG_H
