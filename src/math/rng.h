#pragma once

#include <cstdint>
#include <limits>
#include <nlohmann/detail/meta/type_traits.hpp>
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
 * Upper and lower bounds of RNG range are both inclusive: [min, max].
 * Use operator() on the RNG object to obtain new random numbers.
 * @tparam T Type of number to generate.
 * @tparam D Distribution to use for generating values.
 * @tparam E Type of rng engine to use (std::minstd_rand,
 * std::mt19937, etc).
 */
template <
	typename T, Distribution D = Distribution::Uniform, typename E = std::mt19937,
	typename std::enable_if_t<std::is_arithmetic_v<T>, bool> = true>
class RNG {
private:
	using uniform_type = typename std::conditional_t<
		std::is_floating_point_v<T>, std::uniform_real_distribution<T>,
		typename std::conditional_t<std::is_integral_v<T>, std::uniform_int_distribution<T>, void>>;
	using normal_type = std::normal_distribution<T>;
	// Template which picks correct distribution based on the provided type.
	using distribution = typename std::conditional_t<
		(D == Distribution::Uniform), uniform_type,
		typename std::conditional_t<(D == Distribution::Normal), normal_type, void>>;

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
	explicit RNG(std::uint32_t seed) : RNG{ seed, T{ 0 }, T{ 1 } } {}

	// Custom range seeded distribution.
	// Range: [min, max] (inclusive).
	RNG(std::uint32_t seed, T min, T max) :
		seed_{ seed }, min_{ min }, max_{ max }, generator_{ seed_ } {
		SetupDistribution();
	}

	// Custom range seedless distribution.
	// Range: [min, max] (inclusive).
	RNG(T min, T max) : RNG{ std::invoke(std::random_device{}), min, max } {}

	// Generate a new random number in the specified range.
	[[nodiscard]] T operator()() {
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
		seed_ = new_seed;
		generator_.seed(seed_);
	}

	[[nodiscard]] std::uint32_t GetSeed() const {
		return seed_;
	}

	[[nodiscard]] T GetMin() const {
		return min_;
	}

	[[nodiscard]] T GetMax() const {
		return max_;
	}

	// TODO: Add binary de/serialization.

	template <
		typename BasicJsonType, nlohmann::detail::enable_if_t<
									nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
	friend void to_json(BasicJsonType& nlohmann_json_j, const RNG& nlohmann_json_t) {
		nlohmann_json_j["seed"] = nlohmann_json_t.seed_;
		nlohmann_json_j["min"]	= nlohmann_json_t.min_;
		nlohmann_json_j["max"]	= nlohmann_json_t.max_;
	}

	template <
		typename BasicJsonType, nlohmann::detail::enable_if_t<
									nlohmann::detail::is_basic_json<BasicJsonType>::value, int> = 0>
	friend void from_json(const BasicJsonType& nlohmann_json_j, RNG& nlohmann_json_t) {
		nlohmann_json_t.min_ = nlohmann_json_j["min"];
		nlohmann_json_t.max_ = nlohmann_json_j["max"];
		nlohmann_json_t.SetSeed(nlohmann_json_j["seed"]);
		nlohmann_json_t.SetupDistribution();
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

	std::uint32_t seed_{ std::invoke(std::random_device{}) };

	T min_{ 0 };
	T max_{ 1 };

	// Internal random number generator.
	E generator_{ seed_ };

	// Defined internal distribution.
	// Range: [0, 1] (inclusive).
	distribution distribution_{ min_, max_ };
};

template <typename T>
using Gaussian = RNG<T, Distribution::Normal>;

} // namespace ptgn