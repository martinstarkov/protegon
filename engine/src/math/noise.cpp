#include "math/noise.h"

#include <cstdint>

#include "debug/core/log.h"
#include "debug/runtime/assert.h"
#include "math/math_utils.h"

namespace ptgn {

namespace impl {

void Noise::SetFrequency(float frequency) {
	frequency_ = frequency;
}

[[nodiscard]] float Noise::GetFrequency() const {
	return frequency_;
}

void Noise::SetSeed(std::int32_t seed) {
	seed_ = seed;
}

std::int32_t Noise::GetSeed() const {
	return seed_;
}

} // namespace impl

FractalNoise::FractalNoise() {
	noise_bounding_ = GetNoiseBounding(octaves_, persistence_);
}

void FractalNoise::SetNoiseType(NoiseType type) {
	noise_type_ = type;
}

NoiseType FractalNoise::GetNoiseType() const {
	return noise_type_;
}

void FractalNoise::SetOctaves(std::size_t octaves) {
	PTGN_ASSERT(octaves_ > 0, "Octaves must be set to at least 1");

	if (octaves_ != octaves) {
		octaves_		= octaves;
		noise_bounding_ = GetNoiseBounding(octaves_, persistence_);
	}
}

std::size_t FractalNoise::GetOctaves() const {
	return octaves_;
}

void FractalNoise::SetPersistence(float persistence) {
	PTGN_ASSERT(persistence_ > 0, "Persistence must be positive");
	if (persistence_ != persistence) {
		persistence_	= persistence;
		noise_bounding_ = GetNoiseBounding(octaves_, persistence_);
	}
}

float FractalNoise::GetPersistence() const {
	return persistence_;
}

void FractalNoise::SetWeightedStrength(float weighted_strength) {
	PTGN_ASSERT(
		weighted_strength >= 0.0f && weighted_strength <= 1.0f,
		"Weighted strength must be in range [0.0, 1.0]"
	);
	weighted_strength_ = weighted_strength;
}

float FractalNoise::GetWeightedStrength() const {
	return weighted_strength_;
}

void FractalNoise::SetLacunarity(float lacunarity) {
	PTGN_ASSERT(lacunarity_ > 0);
	lacunarity_ = lacunarity;
}

float FractalNoise::GetLacunarity() const {
	return lacunarity_;
}

float FractalNoise::Get(float x, float y) const {
	return GetImpl(
		x * GetFrequency(), y * GetFrequency(), GetSeed(), noise_type_, octaves_, lacunarity_,
		persistence_, weighted_strength_, noise_bounding_
	);
}

float FractalNoise::Get(float x) const {
	return GetImpl(
		x * GetFrequency(), impl::Noise::default_y, GetSeed(), noise_type_, octaves_, lacunarity_,
		persistence_, weighted_strength_, noise_bounding_
	);
}

float FractalNoise::GetValue(
	float x, float y, std::int32_t seed, float frequency, NoiseType noise_type, std::size_t octaves,
	float lacunarity, float persistence, float weighted_strength
) {
	return GetImpl(
		x * frequency, y * frequency, seed, noise_type, octaves, lacunarity, persistence,
		weighted_strength, GetNoiseBounding(octaves, persistence)
	);
}

float FractalNoise::GetNoiseImpl(float x, float y, std::int32_t seed, NoiseType noise_type) {
	switch (noise_type) {
		case NoiseType::Simplex: return SimplexNoise::GetImpl(x, y, seed);
		case NoiseType::Perlin:	 return PerlinNoise::GetImpl(x, y, seed);
		case NoiseType::Value:	 return ValueNoise::GetImpl(x, y, seed);
		default:				 PTGN_ERROR("Failed to recognize noise type");
	}
}

float FractalNoise::GetImpl(
	float x, float y, std::int32_t seed, NoiseType noise_type, std::size_t octaves,
	float lacunarity, float persistence, float weighted_strength, float noise_bounding
) {
	float sum{ 0.0f };
	float amplitude = noise_bounding;

	for (std::size_t i = 0; i < octaves; i++) {
		float noise	 = GetNoiseImpl(x, y, seed++, noise_type);
		sum			+= noise * amplitude;
		amplitude	*= Lerp(1.0f, Min(noise + 1.0f, 2.0f) * 0.5f, weighted_strength);

		x		  *= lacunarity;
		y		  *= lacunarity;
		amplitude *= persistence;
	}
	return sum * 0.5f + 0.5f;
}

float FractalNoise::GetNoiseBounding(std::size_t octaves, float persistence) {
	float gain		= Abs(persistence);
	float amplitude = gain;
	float max_amplitude{ 1.0f };
	for (std::size_t i{ 1 }; i < octaves; i++) {
		max_amplitude += amplitude;
		amplitude	  *= gain;
	}
	return 1.0f / max_amplitude;
}

float PerlinNoise::Get(float x, float y) const {
	return GetImpl(x * GetFrequency(), y * GetFrequency(), GetSeed());
}

float PerlinNoise::Get(float x) const {
	return GetImpl(x * GetFrequency(), impl::Noise::default_y, GetSeed());
}

float PerlinNoise::GetValue(float x, float y, std::int32_t seed, float frequency) {
	return GetImpl(x * frequency, y * frequency, seed);
}

float PerlinNoise::GetImpl(float x, float y, std::int32_t seed) {
	auto x0 = static_cast<std::int32_t>(Floor(x));
	auto y0 = static_cast<std::int32_t>(Floor(y));

	auto xd0  = x - static_cast<float>(x0);
	auto yd0  = y - static_cast<float>(y0);
	float xd1 = xd0 - 1;
	float yd1 = yd0 - 1;

	float xs = Quintic(xd0);
	float ys = Quintic(yd0);

	x0				*= impl::Noise::prime_x;
	y0				*= impl::Noise::prime_y;
	std::int32_t x1	 = x0 + impl::Noise::prime_x;
	std::int32_t y1	 = y0 + impl::Noise::prime_y;

	float xf0 = Lerp(
		GradientCoordinate(seed, x0, y0, xd0, yd0), GradientCoordinate(seed, x1, y0, xd1, yd0), xs
	);
	float xf1 = Lerp(
		GradientCoordinate(seed, x0, y1, xd0, yd1), GradientCoordinate(seed, x1, y1, xd1, yd1), xs
	);

	return Lerp(xf0, xf1, ys) * 1.4247691104677813f * 0.5f + 0.5f;
}

float ValueNoise::Get(float x, float y) const {
	return GetImpl(x * GetFrequency(), y * GetFrequency(), GetSeed());
}

float ValueNoise::Get(float x) const {
	return GetImpl(x * GetFrequency(), impl::Noise::default_y, GetSeed());
}

float ValueNoise::GetValue(float x, float y, std::int32_t seed, float frequency) {
	return GetImpl(x * frequency, y * frequency, seed);
}

float ValueNoise::GetImpl(float x, float y, std::int32_t seed) {
	auto x0 = static_cast<std::int32_t>(Floor(x));
	auto y0 = static_cast<std::int32_t>(Floor(y));

	float xs = Smoothstep(x - static_cast<float>(x0));
	float ys = Smoothstep(y - static_cast<float>(y0));

	x0	   *= impl::Noise::prime_x;
	y0	   *= impl::Noise::prime_y;
	int x1	= x0 + impl::Noise::prime_x;
	int y1	= y0 + impl::Noise::prime_y;

	float xf0 = Lerp(ValueCoordinate(seed, x0, y0), ValueCoordinate(seed, x1, y0), xs);
	float xf1 = Lerp(ValueCoordinate(seed, x0, y1), ValueCoordinate(seed, x1, y1), xs);

	return Lerp(xf0, xf1, ys) * 0.5f + 0.5f;
}

float SimplexNoise::Get(float x, float y) const {
	return GetImpl(x * GetFrequency(), y * GetFrequency(), GetSeed());
}

float SimplexNoise::Get(float x) const {
	return GetImpl(x * GetFrequency(), impl::Noise::default_y, GetSeed());
}

float SimplexNoise::GetValue(float x, float y, std::int32_t seed, float frequency) {
	return GetImpl(x * frequency, y * frequency, seed);
}

float SimplexNoise::GetImpl(float x, float y, std::int32_t seed) {
	// From: https://github.com/Auburn/FastNoiseLite

	constexpr float SQRT3 = 1.7320508075688772935274463415059f;
	constexpr float G2	  = (3.0f - SQRT3) / 6.0f;

	const float F2	= 0.5f * (SQRT3 - 1.0f);
	float t0		= (x + y) * F2;
	x			   += t0;
	y			   += t0;

	auto i	 = static_cast<std::int32_t>(Floor(x));
	auto j	 = static_cast<std::int32_t>(Floor(y));
	float xi = x - static_cast<float>(i);
	float yi = y - static_cast<float>(j);

	float t	 = (xi + yi) * G2;
	float x0 = xi - t;
	float y0 = yi - t;

	i *= impl::Noise::prime_x;
	j *= impl::Noise::prime_y;

	float n0{ 0.0f };
	float n1{ 0.0f };
	float n2{ 0.0f };

	float a = 0.5f - x0 * x0 - y0 * y0;
	if (a <= 0.0f) {
		n0 = 0.0f;
	} else {
		n0 = (a * a) * (a * a) * GradientCoordinate(seed, i, j, x0, y0);
	}

	float c = 2 * (1 - 2 * G2) * (1 / G2 - 2) * t + (-2 * (1 - 2 * G2) * (1 - 2 * G2) + a);

	if (c <= 0.0f) {
		n2 = 0.0f;
	} else {
		float x2 = x0 + 2.0f * G2 - 1.0f;
		float y2 = y0 + 2.0f * G2 - 1.0f;
		n2		 = (c * c) * (c * c) *
			 GradientCoordinate(seed, i + impl::Noise::prime_x, j + impl::Noise::prime_y, x2, y2);
	}

	if (y0 > x0) {
		float x1 = x0 + G2;
		float y1 = y0 + G2 - 1.0f;
		float b	 = 0.5f - x1 * x1 - y1 * y1;
		if (b <= 0.0f) {
			n1 = 0.0f;
		} else {
			n1 = (b * b) * (b * b) * GradientCoordinate(seed, i, j + impl::Noise::prime_y, x1, y1);
		}
	} else {
		float x1 = x0 + G2 - 1.0f;
		float y1 = y0 + G2;
		float b	 = 0.5f - x1 * x1 - y1 * y1;
		if (b <= 0) {
			n1 = 0;
		} else {
			n1 = (b * b) * (b * b) * GradientCoordinate(seed, i + impl::Noise::prime_x, j, x1, y1);
		}
	}

	return (n0 + n1 + n2) * 99.83685446303647f * 0.5f + 0.5f;
}

} // namespace ptgn