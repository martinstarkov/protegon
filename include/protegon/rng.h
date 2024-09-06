#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <random>
#include <type_traits>

namespace ptgn {

namespace impl {

template <typename T, typename... Ts>
inline constexpr bool invalid_rng_type{ (!std::is_same_v<T, Ts> && ...) };

} // namespace impl

enum class Distribution {
	Uniform = 0,
	Normal	= 1
};

/*
 * Define RNG object by giving it a type to generate from
 * and a range or seed for the distribution.
 * Upper and lower bounds of RNG range are both inclusive: [min, max]
 * Use operator() on the RNG object to obtain new random numbers.
 * @tparam T Type of number to generate
 * @tparam D Distribution to use for generating values
 * @tparam E Type of rng engine to use (std::minstd_rand,
 * std::mt19937, etc)
 */
template <
	typename T, Distribution D = Distribution::Uniform, typename E = std::mt19937,
	typename std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
class RNG {
private:
	using uniform_type = typename std::conditional<
		std::is_floating_point_v<T>, std::uniform_real_distribution<T>,
		typename std::conditional<
			std::is_integral_v<T>, std::uniform_int_distribution<T>, void>::type>::type;
	using normal_type = std::normal_distribution<T>;
	// Template which picks correct distribution based on the provided type.
	using distribution = typename std::conditional<
		(D == Distribution::Uniform), uniform_type,
		typename std::conditional<(D == Distribution::Normal), normal_type, void>::type>::type;

public:
	static_assert(
		impl::invalid_rng_type<T, char, signed char, unsigned char, std::int8_t, std::uint8_t>,
		"RNG does not support the given type"
	);
	// Default range seedless distribution.
	// Range: [0, 1] (inclusive).
	RNG() = default;

	// Default range seeded distribution.
	// Range: [0, 1] (inclusive).
	RNG(std::uint32_t seed) : RNG{ seed, T{ 0 }, T{ 1 } } {}

	// Custom range seeded distribution.
	// Range: [min, max] (inclusive).
	RNG(std::uint32_t seed, T min, T max) : generator_{ seed }, min_{ min }, max_{ max } {
		SetupDistribution();
	}

	// Custom range seedless distribution.
	// Range: [min, max] (inclusive).
	RNG(T min, T max) : RNG{ std::invoke(std::random_device{}), min, max } {}

	// Generate a new random number in the specified range.
	T operator()() {
		auto v = distribution_(generator_);
		// TODO: Check if this assert triggers occasionally for normal distributions. I saw a
		// PTGN_ASSERT(v <= min_ && v >= max_, "RNG failed to generate number in the correct
		// negative value once when generating [0.0f, 1.0f]
		// range");
		if constexpr (D == Distribution::Normal) {
			v = std::clamp(v, min_, max_);
		}
		return v;
	}

	// Change seed of random number generator.
	void SetSeed(std::uint32_t new_seed) {
		generator_.seed(new_seed);
	}

private:
	void SetupDistribution() {
		T a = min_;
		T b = AdjustedMax(max_);
		if constexpr (D == Distribution::Normal) {
			// Mean.
			a = (min_ + max_) / T{ 2 };

			// TODO: Add SFINAE to enable a custom standard deviation to be specified.
			// % of distribution thrown away (in %, no fraction!)
			// i.e. 1/3 = 0.33..% of distribution lost around the edges.
			auto throwaway_range{ T{ 1 } / T{ 3 } };
			b = (max_ - min_) / T{ 2 } * throwaway_range;
		}
		distribution_ = distribution{ a, b };
	}

	[[nodiscard]] T AdjustedMax(T max) {
		if constexpr (std::is_floating_point_v<T>) {
			return std::nextafter(max, std::numeric_limits<T>::epsilon());
		} else {
			return max;
		}
	}

	T min_{};
	T max_{};

	// Internal random number generator.
	E generator_{ std::invoke(std::random_device{}) };

	// Defined internal distribution.
	// Range: [0, 1] (inclusive).
	distribution distribution_{ 0, 1 };
};

template <typename T>
using Gaussian = RNG<T, Distribution::Normal>;

} // namespace ptgn