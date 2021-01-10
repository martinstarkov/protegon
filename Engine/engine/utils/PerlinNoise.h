#pragma once

#include "core/Engine.h"
#include "RNG.h"

#include <cmath>
#include <random>
#include <algorithm>
#include <numeric>
#include <cstdio> 
#include <random> 
#include <functional> 
#include <iostream> 
#include <vector> 
#include <fstream> 

namespace engine {

template<typename T>
class Vec2 {
public:
	Vec2() : x(T(0)), y(T(0)) {}
	Vec2(T xx, T yy) : x(xx), y(yy) {}
	Vec2 operator * (const T& r) const { return Vec2(x * r, y * r); }
	Vec2& operator *= (const T& r) { x *= r, y *= r; return *this; }
	T x, y;
};

typedef Vec2<float> Vec2f;

template<typename T = float>
inline T lerp(const T& lo, const T& hi, const T& t) {
	return lo * (1 - t) + hi * t;
}


inline
float smoothstep(const float& t) {
	return t * t * (3 - 2 * t);
}

class ValueNoise {
public:
	ValueNoise(unsigned size, unsigned seed = 2016) : kMaxTableSize{ size }, kMaxTableSizeMask{ kMaxTableSize - 1 }
	{
		std::mt19937 gen(seed);
		std::uniform_real_distribution<float> distrFloat;
		auto randFloat = std::bind(distrFloat, gen);
		r.resize(kMaxTableSize, 0);
		permutationTable.resize(kMaxTableSize * 2, 0);
		// create an array of random values and initialize permutation table
		for (unsigned k = 0; k < kMaxTableSize; ++k) {
			r[k] = randFloat();
			permutationTable[k] = k;
		}

		// shuffle values of the permutation table
		std::uniform_int_distribution<unsigned> distrUInt;
		auto randUInt = std::bind(distrUInt, gen);
		for (unsigned k = 0; k < kMaxTableSize; ++k) {
			unsigned i = randUInt() & kMaxTableSizeMask;
			std::swap(permutationTable[k], permutationTable[i]);
			permutationTable[k + kMaxTableSize] = permutationTable[k];
		}
	}

	float eval(Vec2f& p) {
		int xi = (int)std::floor(p.x);
		int yi = (int)std::floor(p.y);

		float tx = p.x - xi;
		float ty = p.y - yi;

		int rx0 = xi & kMaxTableSizeMask;
		int rx1 = (rx0 + 1) & kMaxTableSizeMask;
		int ry0 = yi & kMaxTableSizeMask;
		int ry1 = (ry0 + 1) & kMaxTableSizeMask;

		// random values at the corners of the cell using permutation table
		const float& c00 = r[permutationTable[permutationTable[rx0] + ry0]];
		const float& c10 = r[permutationTable[permutationTable[rx1] + ry0]];
		const float& c01 = r[permutationTable[permutationTable[rx0] + ry1]];
		const float& c11 = r[permutationTable[permutationTable[rx1] + ry1]];

		// remapping of tx and ty using the Smoothstep function 
		float sx = smoothstep(tx);
		float sy = smoothstep(ty);

		// linearly interpolate values along the x axis
		float nx0 = lerp(c00, c10, sx);
		float nx1 = lerp(c01, c11, sx);

		// linearly interpolate the nx0/nx1 along they y axis
		return lerp(nx0, nx1, sy);
	}
	unsigned int kMaxTableSize;// = 256 * 2;
	unsigned int kMaxTableSizeMask;// = kMaxTableSize - 1;
	std::vector<float> r; // size: kMaxTableSize
	std::vector<unsigned int> permutationTable; // kMaxTableSize * 2
};





class CustomNoise {
public:
	CustomNoise(float frequency = 1.0f,
				 float amplitude = 1.0f,
				 float lacunarity = 2.0f,
				 float persistence = 0.5f, int seed = 1) :

		mFrequency(frequency),
		mAmplitude(amplitude),
		mLacunarity(lacunarity),
		mPersistence(persistence) {
		perm = CalculatePermutation(seed);
		grad = CalculateGradients(seed);
	}
	std::vector<int> perm;
	std::vector<V2_double> grad;



	/*float noise(float x, float y) {
		RNG rng;
		auto tile_seed = ((int)x & 0xFFFF) << 16 | ((int)y & 0xFFFF);
		rng.SetSeed(tile_seed);
		return rng.RandomDouble(0.0, 1.0);
	}*/
	float Drop(float t) {
		t = abs(t);
		return 1.0f - t * t * t * (t * (t * 6 - 15) + 10);
	}

	float Q(float u, float v) {
		return Drop(u) * Drop(v);
	}
	std::vector<int> CalculatePermutation(int seed = 1) {
		std::vector<int> p;
		p.resize(256);

		// Fill p with values from 0 to 255
		std::iota(p.begin(), p.end(), 0);

		// Initialize a random engine with seed
		std::default_random_engine engine(seed);

		// Suffle  using the above random engine
		std::shuffle(p.begin(), p.end(), engine);

		// Duplicate the permutation vector
		p.insert(p.end(), p.begin(), p.end());
		return p;
	}

	std::vector<V2_double> CalculateGradients(int seed = 1) {
		std::vector<V2_double> grad;
		grad.resize(256);
		RNG rng;
		rng.SetSeed(seed);
		for (auto i = 0; i < grad.size(); i++) {
			V2_double gradient;

			do {
				gradient = V2_double((float)(rng.RandomDouble(0.0, 1.0) * 2 - 1), (float)(rng.RandomDouble(0.0, 1.0) * 2 - 1));
			} while (gradient.MagnitudeSquared() >= 1);

			gradient.Normalized();

			grad[i] = gradient;
		}
		return grad;
	}

	float noise(float x, float y) {
		auto cell = V2_double(engine::math::Floor(x), engine::math::Floor(y));

		auto total = 0.0f;

		auto corners = std::array<V2_double, 4>{ V2_double(0, 0), V2_double(0, 1), V2_double(1, 0), V2_double(1, 1) };

		for (auto n : corners) {
			auto ij = cell + n;
			auto uv = V2_double(x - ij.x, y - ij.x);

			auto index = perm[(int)ij.x % perm.size()];
			index = perm[(index + (int)ij.x) % perm.size()];

			auto gradi = grad[index % grad.size()];

			total += Q((float)uv.x, (float)uv.x) * (float)gradi.DotProduct(uv);
		}

		return (std::max(std::min(total, 1.0f), -1.0f) + 1.0f) / 2.0f;
	}

	float fractal(size_t octaves, float x, float y) {
		float output = 0.f;
		float denom = 0.f;
		float frequency = mFrequency;
		float amplitude = mAmplitude;
		for (size_t i = 0; i < octaves; i++) {
			output += (amplitude * noise(x * frequency, y * frequency));
			denom += amplitude;

			frequency *= mLacunarity;
			amplitude *= mPersistence;
		}

		return (output / denom);
	}
private:
	// Parameters of Fractional Brownian Motion (fBm) : sum of N "octaves" of noise
	float mFrequency;   ///< Frequency ("width") of the first octave of noise (default to 1.0)
	float mAmplitude;   ///< Amplitude ("height") of the first octave of noise (default to 1.0)
	float mLacunarity;  ///< Lacunarity specifies the frequency multiplier between successive octaves (default to 2.0).
	float mPersistence; ///< Persistence is the loss of amplitude between successive octaves (usually 1/lacunarity)
};

class PerlinNoise {
public:
	std::vector<int> p;
	// Generate a new permutation vector based on the value of seed
	PerlinNoise(unsigned int seed) {
		p.resize(256);

		// Fill p with values from 0 to 255
		std::iota(p.begin(), p.end(), 0);

		// Initialize a random engine with seed
		std::default_random_engine engine(seed);

		// Suffle  using the above random engine
		std::shuffle(p.begin(), p.end(), engine);

		// Duplicate the permutation vector
		p.insert(p.end(), p.begin(), p.end());
	}
	//PerlinNoise() {

	//	// Initialize the permutation vector with the reference values
	//	p = {
	//		151,160,137,91,90,15,131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,
	//		8,99,37,240,21,10,23,190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,
	//		35,11,32,57,177,33,88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,
	//		134,139,48,27,166,77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,
	//		55,46,245,40,244,102,143,54, 65,25,63,161,1,216,80,73,209,76,132,187,208, 89,
	//		18,169,200,196,135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,
	//		250,124,123,5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,
	//		189,28,42,223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167,
	//		43,172,9,129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,
	//		97,228,251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,
	//		107,49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	//		138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180 };
	//	// Duplicate the permutation vector
	//	p.insert(p.end(), p.begin(), p.end());
	//}
	double noise(double x, double y, double z) {
		// Find the unit cube that contains the point
		int X = (int)floor(x) & 255;
		int Y = (int)floor(y) & 255;
		int Z = (int)floor(z) & 255;

		// Find relative x, y,z of point in cube
		x -= floor(x);
		y -= floor(y);
		z -= floor(z);

		// Compute fade curves for each of x, y, z
		double u = fade(x);
		double v = fade(y);
		double w = fade(z);

		// Hash coordinates of the 8 cube corners
		int A = p[X] + Y;
		int AA = p[A] + Z;
		int AB = p[A + 1] + Z;
		int B = p[X + 1] + Y;
		int BA = p[B] + Z;
		int BB = p[B + 1] + Z;

		// Add blended results from 8 corners of cube
		double res = lerp(w, lerp(v, lerp(u, grad(p[AA], x, y, z), grad(p[BA], x - 1, y, z)), lerp(u, grad(p[AB], x, y - 1, z), grad(p[BB], x - 1, y - 1, z))), lerp(v, lerp(u, grad(p[AA + 1], x, y, z - 1), grad(p[BA + 1], x - 1, y, z - 1)), lerp(u, grad(p[AB + 1], x, y - 1, z - 1), grad(p[BB + 1], x - 1, y - 1, z - 1))));
		return (res + 1.0) / 2.0;
	}

	double fade(double t) {
		return t * t * t * (t * (t * 6 - 15) + 10);
	}

	double lerp(double t, double a, double b) {
		return a + t * (b - a);
	}

	double grad(int hash, double x, double y, double z) {
		int h = hash & 15;
		// Convert lower 4 bits of hash into 12 gradient directions
		double u = h < 8 ? x : y,
			v = h < 4 ? y : h == 12 || h == 14 ? x : z;
		return ((h & 1) == 0 ? u : -u) + ((h & 2) == 0 ? v : -v);
	}













	// 2D noise variables
	int nOutputWidth = 256;
	int nOutputHeight = 256;
	float* fNoiseSeed2D = nullptr;
	float* fPerlinNoise2D = nullptr;

	// 1D noise variables
	float* fNoiseSeed1D = nullptr;
	float* fPerlinNoise1D = nullptr;
	int nOutputSize = 256;


	int nOctaveCount = 1;
	float fScalingBias = 2.0f;
	int nMode = 1;
	PerlinNoise() {}

	PerlinNoise(AABB info, V2_double tile_size, V2_double chunk_grid) {
		nOutputWidth = (int)info.size.x;
		nOutputHeight = (int)info.size.y;
		RNG rng;
		fNoiseSeed2D = new float[nOutputWidth * nOutputHeight];
		fPerlinNoise2D = new float[nOutputWidth * nOutputHeight];
		/*for (int i = 0; i < nOutputWidth * nOutputHeight; i++) {
			auto noise = (float)rand() / (float)RAND_MAX;
			assert(noise <= 1 && "Noise was not below 1");
			fNoiseSeed2D[i] = noise;
		}*/
		for (int x = 0; x < nOutputWidth; x++) {
			for (int y = 0; y < nOutputHeight; y++) {
				auto index = x + y * nOutputWidth;
				/*V2_double tile_position = V2_double{ (double)x, (double)y } * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				auto tile_seed = ((int)absolute_tile_position.x & 0xFFFF) << 16 | ((int)absolute_tile_position.y & 0xFFFF);
				rng.SetSeed(tile_seed);*/
				double xr = ((double)x + chunk_grid.x) * (double)tile_size.x;
				double yr = ((double)y + chunk_grid.y) * (double)tile_size.y;

				auto tile_seed = ((int)xr & 0xFFFF) << 16 | ((int)yr & 0xFFFF);
				rng.SetSeed(tile_seed);
				auto noise = rng.RandomDouble(0.0, 1.0);
				assert(noise <= 1 && "Noise was not below 1");
				fNoiseSeed2D[index] = (float)noise;
			}
		}

		//nOutputSize = (int)info.size.x;
		//fNoiseSeed1D = new float[nOutputSize];
		//fPerlinNoise1D = new float[nOutputSize];
		//for (int i = 0; i < nOutputSize; i++) {
		//	//auto noise = (float)rand() / (float)RAND_MAX;
		//	auto noise = rng.RandomDouble(0.0, 1.0);
		//	assert(noise <= 1 && "Noise was not below 1");
		//	fNoiseSeed1D[i] = noise;
		//}
	}

	void PerlinNoise1D(int nCount, float* fSeed, int nOctaves, float fBias, float* fOutput) {
		// Used 1D Perlin Noise
		for (int x = 0; x < nCount; x++) {
			float fNoise = 0.0f;
			float fScaleAcc = 0.0f;
			float fScale = 1.0f;

			for (int o = 0; o < nOctaves; o++) {
				int nPitch = nCount >> o;
				int nSample1 = (x / nPitch) * nPitch;
				int nSample2 = (nSample1 + nPitch) % nCount;

				float fBlend = (float)(x - nSample1) / (float)nPitch;

				float fSample = (1.0f - fBlend) * fSeed[nSample1] + fBlend * fSeed[nSample2];

				fScaleAcc += fScale;
				fNoise += fSample * fScale;
				fScale = fScale / fBias;
			}

			// Scale to seed range
			auto noise = fNoise / fScaleAcc;
			assert(noise <= 1 && "Noise was not below 1");
			fOutput[x] = noise;
		}
	}

	void PerlinNoise2D(int nWidth, int nHeight, float* fSeed, int nOctaves, float fBias, float* fOutput) {
		// Used 1D Perlin Noise
		for (int x = 0; x < nWidth; x++)
			for (int y = 0; y < nHeight; y++) {
				float fNoise = 0.0f;
				float fScaleAcc = 0.0f;
				float fScale = 1.0f;

				for (int o = 0; o < nOctaves; o++) {
					int nPitch = nWidth >> o;
					if (nPitch > 0) {
					int nSampleX1 = (x / nPitch) * nPitch;
					int nSampleY1 = (y / nPitch) * nPitch;

					int nSampleX2 = (nSampleX1 + nPitch) % nWidth;
					int nSampleY2 = (nSampleY1 + nPitch) % nWidth;

					float fBlendX = (float)(x - nSampleX1) / (float)nPitch;
					float fBlendY = (float)(y - nSampleY1) / (float)nPitch;

					float fSampleT = (1.0f - fBlendX) * fSeed[nSampleY1 * nWidth + nSampleX1] + fBlendX * fSeed[nSampleY1 * nWidth + nSampleX2];
					float fSampleB = (1.0f - fBlendX) * fSeed[nSampleY2 * nWidth + nSampleX1] + fBlendX * fSeed[nSampleY2 * nWidth + nSampleX2];

					fScaleAcc += fScale;
					fNoise += (fBlendY * (fSampleB - fSampleT) + fSampleT) * fScale;
					fScale = fScale / fBias;
					}
				}

				// Scale to seed range
				auto noise = fNoise / fScaleAcc;
				assert(noise <= 1 && "Noise was not below 1");
				fOutput[y * nWidth + x] = noise;
			}

	}






	PerlinNoise(V2_int output_size, RNG& rng, V2_int grid_coordinate, int seed) : size{ output_size } {
		auto count = size.x * size.y;
		seed2D.resize(count);
		noise2D.resize(count);

		// Seed the noise.
		for (int x = 0; x < size.x; ++x) {
			for (int y = 0; y < size.y; ++y) {
				auto tile_position = grid_coordinate + V2_int{ x, y };
				tile_position *= 16;
				auto tile_seed = (tile_position.x & 0xFFFF) << 16 | (tile_position.y & 0xFFFF);
				rng.SetSeed(tile_seed * seed);
				seed2D[y * size.x + x] = rng.RandomDouble(0, 1);
			}
		}

		/*fNoiseSeed1D = new double[nOutputSize];
		fPerlinNoise1D = new double[nOutputSize];
		for (int i = 0; i < nOutputSize; i++) fNoiseSeed1D[i] = rng.RandomDouble(0, 1);*/
	}
	

	void Generate2D(int octaves = 1, double bias = 2.0) {// Used 1D Perlin Noise
		for (int x = 0; x < size.x; x++)
			for (int y = 0; y < size.y; y++) {
				double fNoise = 0.0;
				double fScaleAcc = 0.0;
				double fScale = 1.0;

				for (int o = 0; o < octaves; o++) {
					int nPitch = size.x >> o;
					assert(size.x >= std::pow(2, o) && "Octive power must be below power of maximum size");
					assert(nPitch > 0 && "Pitch cannot be 0");
					int nSampleX1 = (x / nPitch) * nPitch;
					int nSampleY1 = (y / nPitch) * nPitch;

					int nSampleX2 = (nSampleX1 + nPitch) % size.x;
					int nSampleY2 = (nSampleY1 + nPitch) % size.x;

					double fBlendX = (double)(x - nSampleX1) / (double)nPitch;
					double fBlendY = (double)(y - nSampleY1) / (double)nPitch;

					double fSampleT = (1.0 - fBlendX) * seed2D[nSampleY1 * size.x + nSampleX1] + fBlendX * seed2D[nSampleY1 * size.x + nSampleX2];
					double fSampleB = (1.0 - fBlendX) * seed2D[nSampleY2 * size.x + nSampleX1] + fBlendX * seed2D[nSampleY2 * size.x + nSampleX2];

					fScaleAcc += fScale;
					fNoise += (fBlendY * (fSampleB - fSampleT) + fSampleT) * fScale;
					fScale = fScale / bias;
				}

				// Scale to seed range
				double noise = (fNoise / fScaleAcc);
				if (noise > 1.0) {
					noise = 1.0;
				}
				noise2D[y * size.x + x] = noise;
			}

	}
	double GetNoise2D(V2_int coordinate) {
		assert(coordinate.x < size.x);
		assert(coordinate.y < size.y);
		auto index = coordinate.y * size.x + coordinate.x;
		assert(index < noise2D.size());
		return noise2D[index];
	}
	void PrintNoise(float* array_) {
		LOG("------------------------------------------");
		for (auto i = 0; i < size.x; ++i) {
			for (auto j = 0; j < size.y; ++j) {
				auto index = j * size.x + i;
				//int b = (int)(n * 3);
				LOG_(array_[index] << " ");
			}
			LOG("");
		}
		LOG("------------------------------------------");
	}
	void PrintNoise(const std::vector<double>& noise) {
		LOG("------------------------------------------");
		for (auto i = 0; i < size.x; ++i) {
			for (auto j = 0; j < size.y; ++j) {
				auto index = j * size.x + i;
				double n = noise[index];
				//int b = (int)(n * 3);
				LOG_(n << " ");
			}
			LOG("");
		}
		LOG("------------------------------------------");
	}
	// TODO: Move to private.
	std::vector<double> noise2D;
	std::vector<double> seed2D;
	V2_int size;
private:
};

} // namespace engine