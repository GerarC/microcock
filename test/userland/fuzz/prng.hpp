/**
 * @file prng.hpp
 * @brief Deterministic pseudo random generator of the syscall fuzzer (xorshift32).
 *
 * The same seed gives the same sequence, so a failing run can be repeated. Not suitable for anything that needs unpredictability. Tested on the host.
 */
#ifndef FUZZ_PRNG_HPP
#define FUZZ_PRNG_HPP

#include <stddef.h>
#include <stdint.h>

namespace fuzz {

/** State used when the seed is 0, because xorshift32 never leaves state 0 (golden ratio constant). */
constexpr uint32_t FALLBACK_STATE = 0x9E3779B9;
/** First xorshift step: left shift. */
constexpr uint32_t FIRST_SHIFT = 13;
/** Second xorshift step: right shift. */
constexpr uint32_t SECOND_SHIFT = 17;
/** Third xorshift step: left shift. */
constexpr uint32_t THIRD_SHIFT = 5;
/** Range of the percentages given to `chance`. */
constexpr uint32_t PERCENT_SCALE = 100;

/** Xorshift32 generator with period 2^32 - 1. Not thread safe: use one per thread. */
class Prng {
  public:
	/**
	 * @brief Creates a generator.
	 * @param seed Initial state; 0 is replaced by `FALLBACK_STATE`.
	 */
	constexpr explicit Prng(uint32_t seed) : state(seed != 0 ? seed : FALLBACK_STATE) {}

	/**
	 * @brief Advances the generator.
	 * @return The next 32 bit value.
	 */
	uint32_t next() {
		state ^= state << FIRST_SHIFT;
		state ^= state >> SECOND_SHIFT;
		state ^= state << THIRD_SHIFT;
		return state;
	}

	/**
	 * @brief Draws a number in a range.
	 * @param limit Exclusive upper bound; 0 returns 0 without advancing.
	 * @return A value from 0 to `limit - 1` (slightly biased when `limit` is not a power of two).
	 */
	uint32_t below(uint32_t limit) { return limit == 0 ? 0 : next() % limit; }

	/**
	 * @brief Random yes or no.
	 * @param percent Probability in percent, 0 to 100.
	 * @return True with that probability.
	 */
	bool chance(uint32_t percent) { return below(PERCENT_SCALE) < percent; }

	/**
	 * @brief Picks one element of a constant array.
	 * @tparam Type Element type.
	 * @tparam Count Number of elements, at least 1.
	 * @param values Array to pick from.
	 * @return A copy of a random element.
	 */
	template <typename Type, size_t Count> Type pick(const Type (&values)[Count]) {
		return values[below(static_cast<uint32_t>(Count))];
	}

  private:
	/** Current generator state; never 0. */
	uint32_t state;
};

} // namespace fuzz

#endif // !FUZZ_PRNG_HPP
