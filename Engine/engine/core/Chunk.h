#pragma once

#include <vector> // std::vector
#include <cstdlib> // std::size_t
#include "utils/Vector2.h"
#include "ecs/ECS.h" // ecs::Entity, ecs::Manager
#include "renderer/AABB.h"

namespace engine {

class BaseChunk {
public:
	virtual ecs::Entity GetEntity(V2_int relative_coordinate) const = 0;
	virtual ecs::Entity& GetEntity(V2_int relative_coordinate) = 0;
	virtual const AABB& GetInfo() const = 0;
	virtual void Unload() = 0;
	virtual void Init(AABB chunk_info, V2_int tile_size, ecs::Manager* manager) = 0;
	virtual void Generate() = 0;
	virtual bool MatchesPotentialChunk(const AABB& potential_chunk, const V2_int& tile_size, const ecs::Manager* manager) const = 0;
};

class Chunk : public BaseChunk {
public:
	Chunk() = default;
	~Chunk();
	virtual void Init(AABB chunk_info, V2_int tile_size, ecs::Manager* manager) override final;
	virtual ecs::Entity GetEntity(V2_int relative_coordinate) const override final;
	virtual ecs::Entity& GetEntity(V2_int relative_coordinate) override final;
	virtual const AABB& GetInfo() const override final;
	// Destroys all grid entities.
	virtual void Unload() override final;
	friend bool operator==(const Chunk& a, const Chunk& b);
	// Used for comparing a potential chunk with current existing chunk. This allows for quickly culling chunks that are loaded already.
	virtual bool MatchesPotentialChunk(const AABB& potential_chunk, const V2_int& tile_size, const ecs::Manager* manager) const override final;
protected:
	std::size_t GetIndex(V2_int relative_coordinate) const;
	ecs::Manager* manager{ nullptr };
	V2_int tile_size{};
	V2_int tile_count{};
	AABB info{};
	std::vector<ecs::Entity> grid;
};

// Used for comparing existing chunks to newly loaded ones.
inline bool operator==(const Chunk& a, const Chunk& b) { return a.info == b.info && a.tile_size == b.tile_size; }

} // namespace engine