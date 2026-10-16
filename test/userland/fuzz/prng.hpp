#ifndef FUZZ_PRNG_HPP
#define FUZZ_PRNG_HPP

#include <stddef.h>
#include <stdint.h>

namespace fuzz {

constexpr uint32_t FALLBACK_STATE = 0x9E3779B9;
constexpr uint32_t FIRST_SHIFT = 13;
constexpr uint32_t SECOND_SHIFT = 17;
constexpr uint32_t THIRD_SHIFT = 5;
constexpr uint32_t PERCENT_SCALE = 100;

class Prng {
  public:
	constexpr explicit Prng(uint32_t seed) : state(seed != 0 ? seed : FALLBACK_STATE) {}

	uint32_t next() {
		state ^= state << FIRST_SHIFT;
		state ^= state >> SECOND_SHIFT;
		state ^= state << THIRD_SHIFT;
		return state;
	}

	uint32_t below(uint32_t limit) { return limit == 0 ? 0 : next() % limit; }

	bool chance(uint32_t percent) { return below(PERCENT_SCALE) < percent; }

	template <typename Type, size_t Count> Type pick(const Type (&values)[Count]) {
		return values[below(static_cast<uint32_t>(Count))];
	}

  private:
	uint32_t state;
};

} // namespace fuzz

#endif // !FUZZ_PRNG_HPP
