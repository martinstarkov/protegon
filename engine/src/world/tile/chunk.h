#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

#include "core/entity.h"
#include "math/noise.h"
#include "math/vector2.h"
#include "serialization/json.h"

namespace ptgn {

class Camera;
class Manager;
class ChunkManager;

class Chunk {
public:
	Chunk() = default;
	explicit Chunk(const std::vector<Entity>& entities);
	Chunk(const json& j, Manager& manager);
	Chunk(const Chunk&)			   = delete;
	Chunk& operator=(const Chunk&) = delete;
	Chunk(Chunk&& other) noexcept;
	Chunk& operator=(Chunk&& other) noexcept;
	~Chunk();

	// @return True if the chunk has changed from its base generated state and should be serialized,
	// false otherwise.
	[[nodiscard]] bool HasChanged() const;

	// Flag the chunk as changed which will lead to it being serialized on the next chunk manager
	// update.
	void FlagAsChanged(bool changed = true);

	[[nodiscard]] json Serialize() const;

	void Deserialize(const json& j, Manager& manager);

	std::vector<Entity> entities;

private:
	friend class ChunkManager;

	bool has_changed_{ false };
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

	void Update(Manager& manager, const Camera& camera);

	std::unordered_map<V2_int, Chunk> chunks;
	V2_int tile_size{ 64, 64 };
	V2_int chunk_size{ 16, 16 };

	void AddNoiseLayer(const NoiseLayer& noise_layer);

	std::unordered_map<V2_int, json> chunk_cache;

private:
	V2_int previous_min_;
	V2_int previous_max_;

	void DrawDebugChunkBorders() const;

	// @param chunk_padding Number of additional chunks on each side that are loaded past the camera
	// view rectangle.
	void GetBounds(
		V2_int& out_min, V2_int& out_max, const Camera& camera, const V2_int& chunk_padding
	) const;

	[[nodiscard]] std::vector<Entity> GenerateEntities(const V2_int& chunk_coordinate) const;

	std::vector<NoiseLayer> noise_layers_;
};

} // namespace ptgn