#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "core/entity.h"
#include "math/noise.h"
#include "math/vector2.h"

// TODO: Add serialization.

namespace ptgn {

class Camera;

class Chunk {
public:
	explicit Chunk(const std::vector<Entity>& entities);
	Chunk(const Chunk&)			   = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&& other) noexcept;
	Chunk& operator=(Chunk&& other) noexcept;
	~Chunk();

	std::vector<Entity> entities;
};

struct NoiseLayer {
	NoiseLayer() = default;

	NoiseLayer(
		const FractalNoise& fractal_noise,
		const std::function<Entity(V2_int, float)>& creation_callback
	) :
		noise{ fractal_noise }, callback{ creation_callback } {}

	FractalNoise noise;
	// Out: entity, In: coordinate, noise value
	std::function<Entity(V2_int, float)> callback;

	Entity GetEntity(const V2_int& tile_coordinate, const V2_int& tile_size) const;
};

class ChunkManager {
public:
	ChunkManager()								 = default;
	ChunkManager(const ChunkManager&)			 = delete;
	ChunkManager& operator=(const ChunkManager&) = delete;
	ChunkManager(ChunkManager&& other) noexcept;
	ChunkManager& operator=(ChunkManager&& other) noexcept;
	~ChunkManager();

	void Update(const Camera& camera);

	std::unordered_map<V2_int, Chunk> chunks;
	std::vector<NoiseLayer> noise_layers;
	V2_int tile_size{ 64, 64 };
	V2_int chunk_size{ 16, 16 };

private:
	[[nodiscard]] std::vector<Entity> GenerateEntities(const V2_int& chunk_coordinate) const;
};

} // namespace ptgn