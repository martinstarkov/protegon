#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <numeric>
#include <utility>
#include <vector>

#include "math/math.h"
#include "math/rng.h"
#include "math/vector2.h"
#include "common/assert.h"

namespace ptgn {

namespace impl {

class Noise {
public:
	virtual ~Noise() = default;

	void SetFrequency(float frequency);
	[[nodiscard]] float GetFrequency() const;

	void SetSeed(std::int32_t seed);
	[[nodiscard]] std::int32_t GetSeed() const;

	[[nodiscard]] virtual float Get(float x, float y) const = 0;
	[[nodiscard]] virtual float Get(float x) const			= 0;

protected:
	[[nodiscard]] static float ValueCoordinate(
		std::int32_t seed, std::int32_t xPrimed, std::int32_t yPrimed
	) {
		std::int32_t hash  = Hash(seed, xPrimed, yPrimed);
		hash			  *= hash;
		hash			  ^= hash << 19;
		return static_cast<float>(hash) * (1.0f / 2147483648.0f);
	}

	[[nodiscard]] static float GradientCoordinate(
		std::int32_t seed, std::int32_t xPrimed, std::int32_t yPrimed, float xd, float yd
	) {
		std::int32_t hash  = Hash(seed, xPrimed, yPrimed);
		hash			  ^= hash >> 15;
		hash			  &= 127 << 1;

		PTGN_ASSERT(
			hash < static_cast<std::int32_t>(gradients.size()) &&
			(hash | 1) < static_cast<std::int32_t>(gradients.size())
		);

		float xg = gradients[static_cast<std::size_t>(hash)];
		float yg = gradients[static_cast<std::size_t>(hash) | 1];

		return xd * xg + yd * yg;
	}

	static constexpr std::int32_t prime_x = 501125321;
	static constexpr std::int32_t prime_y = 1136930381;
	static constexpr std::int32_t prime_z = 1720413743;

	static constexpr float default_y = 0.12345f; /* default y for 1D noise */

private:
	static constexpr std::array<float, 256> gradients{
		0.130526192220052f,	 0.99144486137381f,	  0.38268343236509f,   0.923879532511287f,
		0.608761429008721f,	 0.793353340291235f,  0.793353340291235f,  0.608761429008721f,
		0.923879532511287f,	 0.38268343236509f,	  0.99144486137381f,   0.130526192220051f,
		0.99144486137381f,	 -0.130526192220051f, 0.923879532511287f,  -0.38268343236509f,
		0.793353340291235f,	 -0.60876142900872f,  0.608761429008721f,  -0.793353340291235f,
		0.38268343236509f,	 -0.923879532511287f, 0.130526192220052f,  -0.99144486137381f,
		-0.130526192220052f, -0.99144486137381f,  -0.38268343236509f,  -0.923879532511287f,
		-0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
		-0.923879532511287f, -0.38268343236509f,  -0.99144486137381f,  -0.130526192220052f,
		-0.99144486137381f,	 0.130526192220051f,  -0.923879532511287f, 0.38268343236509f,
		-0.793353340291235f, 0.608761429008721f,  -0.608761429008721f, 0.793353340291235f,
		-0.38268343236509f,	 0.923879532511287f,  -0.130526192220052f, 0.99144486137381f,
		0.130526192220052f,	 0.99144486137381f,	  0.38268343236509f,   0.923879532511287f,
		0.608761429008721f,	 0.793353340291235f,  0.793353340291235f,  0.608761429008721f,
		0.923879532511287f,	 0.38268343236509f,	  0.99144486137381f,   0.130526192220051f,
		0.99144486137381f,	 -0.130526192220051f, 0.923879532511287f,  -0.38268343236509f,
		0.793353340291235f,	 -0.60876142900872f,  0.608761429008721f,  -0.793353340291235f,
		0.38268343236509f,	 -0.923879532511287f, 0.130526192220052f,  -0.99144486137381f,
		-0.130526192220052f, -0.99144486137381f,  -0.38268343236509f,  -0.923879532511287f,
		-0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
		-0.923879532511287f, -0.38268343236509f,  -0.99144486137381f,  -0.130526192220052f,
		-0.99144486137381f,	 0.130526192220051f,  -0.923879532511287f, 0.38268343236509f,
		-0.793353340291235f, 0.608761429008721f,  -0.608761429008721f, 0.793353340291235f,
		-0.38268343236509f,	 0.923879532511287f,  -0.130526192220052f, 0.99144486137381f,
		0.130526192220052f,	 0.99144486137381f,	  0.38268343236509f,   0.923879532511287f,
		0.608761429008721f,	 0.793353340291235f,  0.793353340291235f,  0.608761429008721f,
		0.923879532511287f,	 0.38268343236509f,	  0.99144486137381f,   0.130526192220051f,
		0.99144486137381f,	 -0.130526192220051f, 0.923879532511287f,  -0.38268343236509f,
		0.793353340291235f,	 -0.60876142900872f,  0.608761429008721f,  -0.793353340291235f,
		0.38268343236509f,	 -0.923879532511287f, 0.130526192220052f,  -0.99144486137381f,
		-0.130526192220052f, -0.99144486137381f,  -0.38268343236509f,  -0.923879532511287f,
		-0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
		-0.923879532511287f, -0.38268343236509f,  -0.99144486137381f,  -0.130526192220052f,
		-0.99144486137381f,	 0.130526192220051f,  -0.923879532511287f, 0.38268343236509f,
		-0.793353340291235f, 0.608761429008721f,  -0.608761429008721f, 0.793353340291235f,
		-0.38268343236509f,	 0.923879532511287f,  -0.130526192220052f, 0.99144486137381f,
		0.130526192220052f,	 0.99144486137381f,	  0.38268343236509f,   0.923879532511287f,
		0.608761429008721f,	 0.793353340291235f,  0.793353340291235f,  0.608761429008721f,
		0.923879532511287f,	 0.38268343236509f,	  0.99144486137381f,   0.130526192220051f,
		0.99144486137381f,	 -0.130526192220051f, 0.923879532511287f,  -0.38268343236509f,
		0.793353340291235f,	 -0.60876142900872f,  0.608761429008721f,  -0.793353340291235f,
		0.38268343236509f,	 -0.923879532511287f, 0.130526192220052f,  -0.99144486137381f,
		-0.130526192220052f, -0.99144486137381f,  -0.38268343236509f,  -0.923879532511287f,
		-0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
		-0.923879532511287f, -0.38268343236509f,  -0.99144486137381f,  -0.130526192220052f,
		-0.99144486137381f,	 0.130526192220051f,  -0.923879532511287f, 0.38268343236509f,
		-0.793353340291235f, 0.608761429008721f,  -0.608761429008721f, 0.793353340291235f,
		-0.38268343236509f,	 0.923879532511287f,  -0.130526192220052f, 0.99144486137381f,
		0.130526192220052f,	 0.99144486137381f,	  0.38268343236509f,   0.923879532511287f,
		0.608761429008721f,	 0.793353340291235f,  0.793353340291235f,  0.608761429008721f,
		0.923879532511287f,	 0.38268343236509f,	  0.99144486137381f,   0.130526192220051f,
		0.99144486137381f,	 -0.130526192220051f, 0.923879532511287f,  -0.38268343236509f,
		0.793353340291235f,	 -0.60876142900872f,  0.608761429008721f,  -0.793353340291235f,
		0.38268343236509f,	 -0.923879532511287f, 0.130526192220052f,  -0.99144486137381f,
		-0.130526192220052f, -0.99144486137381f,  -0.38268343236509f,  -0.923879532511287f,
		-0.608761429008721f, -0.793353340291235f, -0.793353340291235f, -0.608761429008721f,
		-0.923879532511287f, -0.38268343236509f,  -0.99144486137381f,  -0.130526192220052f,
		-0.99144486137381f,	 0.130526192220051f,  -0.923879532511287f, 0.38268343236509f,
		-0.793353340291235f, 0.608761429008721f,  -0.608761429008721f, 0.793353340291235f,
		-0.38268343236509f,	 0.923879532511287f,  -0.130526192220052f, 0.99144486137381f,
		0.38268343236509f,	 0.923879532511287f,  0.923879532511287f,  0.38268343236509f,
		0.923879532511287f,	 -0.38268343236509f,  0.38268343236509f,   -0.923879532511287f,
		-0.38268343236509f,	 -0.923879532511287f, -0.923879532511287f, -0.38268343236509f,
		-0.923879532511287f, 0.38268343236509f,	  -0.38268343236509f,  0.923879532511287f,
	};

	[[nodiscard]] static std::int32_t Hash(
		std::int32_t seed, std::int32_t xPrimed, std::int32_t yPrimed
	) {
		std::int32_t hash = seed ^ xPrimed ^ yPrimed;

		hash *= 0x27d4eb2d;
		return hash;
	}

	std::int32_t seed_{ 0 };

	// Sampling rate of the first layer of noise as a % of the provided noise array.
	// Lower value means the initial noise layer has a higher noise frequency.
	float frequency_{ 0.01f };
};

} // namespace impl

class PerlinNoise : public impl::Noise {
public:
	[[nodiscard]] float Get(float x, float y) const final;
	[[nodiscard]] float Get(float x) const final;
	[[nodiscard]] static float GetValue(
		float x, float y, std::int32_t seed = 0, float frequency = 0.01f
	);

private:
	friend class FractalNoise;
	// x and y already multiplied by frequency.
	[[nodiscard]] static float GetImpl(float x, float y, std::int32_t seed);
};

class ValueNoise : public impl::Noise {
public:
	[[nodiscard]] float Get(float x, float y) const final;
	[[nodiscard]] float Get(float x) const final;
	[[nodiscard]] static float GetValue(
		float x, float y, std::int32_t seed = 0, float frequency = 0.01f
	);

private:
	friend class FractalNoise;
	// x and y already multiplied by frequency.
	[[nodiscard]] static float GetImpl(float x, float y, std::int32_t seed);
};

// Technically OpenSimplex noise but "Open" removed for brevity.
class SimplexNoise : public impl::Noise {
public:
	[[nodiscard]] float Get(float x, float y) const final;
	[[nodiscard]] float Get(float x) const final;
	[[nodiscard]] static float GetValue(
		float x, float y, std::int32_t seed = 0, float frequency = 0.01f
	);

private:
	friend class FractalNoise;
	// x and y already multiplied by frequency.
	[[nodiscard]] static float GetImpl(float x, float y, std::int32_t seed);
};

enum class NoiseType {
	Perlin,
	Value,
	Simplex // Technically OpenSimplex noise but "Open" removed for brevity.
};

class FractalNoise : public impl::Noise {
public:
	FractalNoise();

	void SetNoiseType(NoiseType type);
	[[nodiscard]] NoiseType GetNoiseType() const;

	// Number of layers of noise added on top of each other. Lower value means less higher frequency
	// noise layers.
	void SetOctaves(std::size_t octaves);
	[[nodiscard]] std::size_t GetOctaves() const;

	// Amount by which the amplitude of each successive layer of noise is multiplied. Lower value
	// means less high frequency noise. Also sometimes called fractal gain.
	// Increasing the value of persistence increases the influence of small features on the overall
	// noise map.
	void SetPersistence(float persistence);
	[[nodiscard]] float GetPersistence() const;

	// Higher values mean higher octaves have less impact if lower octaves have a large impact.
	void SetWeightedStrength(float weighted_strength);
	[[nodiscard]] float GetWeightedStrength() const;

	// Amount by which the sampling rate (frequency) of each successive layer of noise is
	// multiplied. Lower value means the noise frequency of each noise layer increases slower.
	// Increasing the value of lacunarity increases the number of small features.
	void SetLacunarity(float lacunarity);
	[[nodiscard]] float GetLacunarity() const;

	[[nodiscard]] float Get(float x, float y) const final;
	[[nodiscard]] float Get(float x) const final;

	[[nodiscard]] static float GetValue(
		float x, float y, std::int32_t seed = 0, float frequency = 0.01f,
		NoiseType noise_type = NoiseType::Perlin, std::size_t octaves = 3, float lacunarity = 2.0f,
		float persistence = 0.5f, float weighted_strength = 0.0f
	);

private:
	// x and y already multiplied by frequency.
	[[nodiscard]] static float GetImpl(
		float x, float y, std::int32_t seed, NoiseType noise_type, std::size_t octaves,
		float lacunarity, float persistence, float weighted_strength, float noise_bounding
	);

	[[nodiscard]] static float GetNoiseImpl(
		float x, float y, std::int32_t seed, NoiseType noise_type
	);

	[[nodiscard]] static float GetNoiseBounding(std::size_t octaves, float persistence);

	// 1 / maximum value of noise possible with given fractal properties.
	float noise_bounding_{ 1.0f / 1.75f };

	// Number of layers of noise added on top of each other. Lower value means less higher frequency
	// noise layers.
	std::size_t octaves_{ 3 };

	// Amount by which the sampling rate (frequency) of each successive layer of noise is
	// multiplied. Lower value means the noise frequency of each noise layer increases slower.
	float lacunarity_{ 2.0f };

	// Amount by which the amplitude of each successive layer of noise is multiplied. Lower value
	// means less high frequency noise. Also sometimes called fractal gain.
	float persistence_{ 0.5f };

	// Higher values mean higher octaves have less impact if lower octaves have a large impact.
	float weighted_strength_{ 0.0f };

	NoiseType noise_type_{ NoiseType::Perlin };
};

} // namespace ptgn
