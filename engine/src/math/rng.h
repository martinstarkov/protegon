#pragma once

#include <concepts>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <type_traits>
#include <vector>

#include "common/assert.h"
#include "serialization/enum.h"
#include "serialization/json.h"

namespace ptgn {

namespace impl {

template <typename T, typename... Ts>
inline constexpr bool invalid_rng_type = (std::same_as<T, Ts> || ...);

template <typename T>
concept RNGType = std::is_arithmetic_v<T> &&
				  !invalid_rng_type<T, char, signed char, unsigned char, std::int8_t, std::uint8_t>;

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
template <impl::RNGType T, Distribution D = Distribution::Uniform, typename E = std::mt19937>
class RNG {
private:
	using uniform_type = std::conditional_t<
		std::is_floating_point_v<T>, std::uniform_real_distribution<T>,
		std::uniform_int_distribution<T>>;

	using normal_type = std::normal_distribution<T>;

	// Template which picks correct distribution based on the provided type.
	using distribution = typename std::conditional_t<
		(D == Distribution::Uniform), uniform_type,
		std::conditional_t<(D == Distribution::Normal), normal_type, void>>;

public:
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
	RNG(T min, T max) : RNG{ std::random_device{}(), min, max } {}

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

	// Change the range of the random number generator.
	void SetRange(T min, T max) {
		min_ = min;
		max_ = max;
		SetupDistribution();
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

	friend void to_json(json& j, const RNG& rng) {
		j["seed"] = rng.GetSeed();
		j["min"]  = rng.GetMin();
		j["max"]  = rng.GetMax();
	}

	friend void from_json(const json& j, RNG& rng) {
		rng.min_ = j.at("min").get<T>();
		rng.max_ = j.at("max").get<T>();
		rng.SetSeed(j.at("seed").get<std::uint32_t>());
		rng.SetupDistribution();
	}

private:
	void SetupDistribution() {
		PTGN_ASSERT(min_ <= max_);
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

	std::uint32_t seed_{ std::random_device{}() };

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

PTGN_SERIALIZER_REGISTER_ENUM(
	Distribution, { { Distribution::Uniform, "uniform" }, { Distribution::Normal, "normal" } }
);

// @return True for "heads", false for "tails"
[[nodiscard]] bool FlipCoin();

template <impl::RNGType T = std::int32_t>
[[nodiscard]] T RandomNumber() {
	static RNG<T> rng{ std::numeric_limits<T>::min(), std::numeric_limits<T>::max() };
	return rng();
}

template <typename T>
class RandomPicker {
public:
	// Accepts any number of arguments to initialize the item list
	template <typename... Args>
	explicit RandomPicker(Args&&... args) :
		items_{ std::forward<Args>(args)... }, rng_(0, Size() - 1) {}

	// @return The next random element removed from the RandomPicker, or std::nullopt if none are
	// available.
	std::optional<T> Next() {
		if (IsEmpty()) {
			return std::nullopt;
		}

		std::size_t idx{ rng_() };
		T value{ std::move(items_[idx]) };
		items_.erase(items_.begin() + idx);

		// Update RNG range if items remain
		if (!IsEmpty()) {
			rng_.SetRange(0, Size() - 1);
		}

		return value;
	}

	// @return True if the RandomPicker no longer has any items, false otherwise.
	[[nodiscard]] bool IsEmpty() const {
		return items_.empty();
	}

	// @return How many items remain in the RandomPicker.
	[[nodiscard]] std::size_t Size() const {
		return items_.size();
	}

	template <typename Func>
	void ForEach(Func func) {
		for (auto i = 0; i < Size(); i++) {
			func(items_[i]);
		}
	}

private:
	std::vector<T> items_;
	RNG<std::size_t> rng_;
};

} // namespace ptgn