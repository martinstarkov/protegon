#pragma once

#include "core/Engine.h"
#include "RNG.h"

namespace engine {

class PerlinNoise {
public:
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


	PerlinNoise(AABB info, V2_double tile_size) {
		nOutputWidth = (int)info.size.x;
		nOutputHeight = (int)info.size.y;
		engine::RNG rng;
		fNoiseSeed2D = new float[nOutputWidth * nOutputHeight];
		fPerlinNoise2D = new float[nOutputWidth * nOutputHeight];
		/*for (int i = 0; i < nOutputWidth * nOutputHeight; i++) {
			auto noise = (float)rand() / (float)RAND_MAX;
			assert(noise <= 1 && "Noise was not below 1");
			fNoiseSeed2D[i] = noise;
		}*/
		for (int x = 0; x < nOutputWidth; x++) {
			for (int y = 0; y < nOutputWidth; y++) {
				auto index = x + y * nOutputWidth;
				V2_double tile_position = V2_double{ (double)x, (double)y } * tile_size;
				V2_double absolute_tile_position = tile_position + info.position;
				auto tile_seed = ((int)absolute_tile_position.x & 0xFFFF) << 16 | ((int)absolute_tile_position.y & 0xFFFF);
				rng.SetSeed(tile_seed);
				auto noise = rng.RandomDouble(0.0, 1.0);
				assert(noise <= 1 && "Noise was not below 1");
				fNoiseSeed2D[index] = (float)noise;
			}
		}

		nOutputSize = (int)info.size.x;
		fNoiseSeed1D = new float[nOutputSize];
		fPerlinNoise1D = new float[nOutputSize];
		for (int i = 0; i < nOutputSize; i++) {
			auto noise = (float)rand() / (float)RAND_MAX;
			assert(noise <= 1 && "Noise was not below 1");
			fNoiseSeed1D[i] = noise;
		}
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






	PerlinNoise() {}
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