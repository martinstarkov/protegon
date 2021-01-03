#pragma once

#include "core/Engine.h"
#include "RNG.h"

namespace engine {

class PerlinNoise {
public:
	PerlinNoise(V2_int output_size, RNG& rng) : size{ output_size } {
		auto count = output_size.x * output_size.y;
		seed2D.resize(count);
		noise2D.resize(count);
		for (int i = 0; i < count; i++) seed2D[i] = rng.RandomDouble(0, 1);

		/*fNoiseSeed1D = new double[nOutputSize];
		fPerlinNoise1D = new double[nOutputSize];
		for (int i = 0; i < nOutputSize; i++) fNoiseSeed1D[i] = rng.RandomDouble(0, 1);*/
	}
	//void PerlinNoise1D(int nCount, float* fSeed, int nOctaves, float fBias, float* fOutput) {
	//	// Used 1D Perlin Noise
	//	for (int x = 0; x < nCount; x++) {
	//		float fNoise = 0.0f;
	//		float fScaleAcc = 0.0f;
	//		float fScale = 1.0f;
	//		for (int o = 0; o < nOctaves; o++) {
	//			int nPitch = nCount >> o;
	//			int nSample1 = (x / nPitch) * nPitch;
	//			int nSample2 = (nSample1 + nPitch) % nCount;
	//			float fBlend = (float)(x - nSample1) / (float)nPitch;
	//			float fSample = (1.0f - fBlend) * fSeed[nSample1] + fBlend * fSeed[nSample2];
	//			fScaleAcc += fScale;
	//			fNoise += fSample * fScale;
	//			fScale = fScale / fBias;
	//		}
	//		// Scale to seed range
	//		fOutput[x] = fNoise / fScaleAcc;
	//	}
	//}

	void Generate2D(int octaves = 1, double bias = 2.0) {
		// Used 1D Perlin Noise
		for (int x = 0; x < size.x; x++)
			for (int y = 0; y < size.y; y++) {
				double noise = 0.0;
				double scale_accumulator = 0.0;
				double scale = 1.0;

				for (int o = 0; o < octaves; o++) {
					// TODO: Fix pitch.
					int pitch = size.x >> o;
					assert(size.x >= std::pow(2, o) && "Octive power must be below power of maximum size");
					assert(pitch > 0 && "Pitch cannot be 0");
					V2_int sample1 = { (x / pitch) * pitch, (y / pitch) * pitch };
					V2_int sample2 = { (sample1.x + pitch) % size.x, (sample1.y + pitch) % size.x };

					V2_double blend = { (double)(x - sample1.x) / (double)pitch, (double)(y - sample1.y) / (double)pitch };

					double sampleT = (1.0 - blend.x) * seed2D[sample1.y * size.x + sample1.x] + blend.x * seed2D[sample1.y * size.x + sample2.x];
					double sampleB = (1.0 - blend.x) * seed2D[sample2.y * size.x + sample1.x] + blend.x * seed2D[sample2.y * size.x + sample2.x];

					scale_accumulator += scale;
					noise += (blend.y * (sampleB - sampleT) + sampleT) * scale;
					scale = scale / bias;
				}

				// Scale to seed range
				noise2D[y * size.x + x] = noise / scale_accumulator;
			}

	}
	double GetNoise2D(V2_int coordinate) {
		assert(coordinate.x < size.x);
		assert(coordinate.y < size.y);
		auto index = coordinate.y * size.x + coordinate.x;
		assert(index < noise2D.size());
		return noise2D[index];
	}
	// TODO: Move to private.
	std::vector<double> noise2D;
	V2_int size;
private:
	std::vector<double> seed2D;
};

}