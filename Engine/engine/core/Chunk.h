#pragma once

#include <vector> // std::vector
#include <cstdlib> // std::size_t
#include "math/Vector2.h"
#include "ecs/ECS.h" // ecs::Entity, ecs::Manager
#include "renderer/AABB.h"
#include "renderer/Texture.h"

// TODO: Move to procedural folder.

namespace engine {

class Scene;

class BaseChunk {
public:
	virtual const ecs::Entity& GetEntity(const V2_int& relative_coordinate) const = 0;
	virtual ecs::Entity& GetEntity(const V2_int& relative_coordinate) = 0;
	virtual const AABB& GetInfo() const = 0;
	virtual void Unload() = 0;
	virtual void Init(const AABB& chunk_info, const V2_int& tile_size, Scene* scene) = 0;
	virtual void Generate(int seed, int octave, double bias) = 0;
	virtual void Update() = 0;
	virtual void Render() = 0;
	virtual ~BaseChunk() = default;
};

class Chunk : public BaseChunk {
public:
	virtual ~Chunk() override;
	virtual void Init(const AABB& chunk_info, const V2_int& tile_size, Scene* scene) override final;
	virtual const ecs::Entity& GetEntity(const V2_int& relative_coordinate) const override final;
	virtual ecs::Entity& GetEntity(const V2_int& relative_coordinate) override final;
	virtual const AABB& GetInfo() const override final;
	virtual void Update() override final;
	virtual void Render() override final;
	// Destroys all grid entities.
	virtual void Unload() override final;
	friend bool operator==(const Chunk& a, const Chunk& b);
	ecs::Manager manager;
	bool new_chunk = true;
protected:
	Scene* scene = nullptr;
	Texture chunk = nullptr;
	std::size_t GetIndex(const V2_int& relative_coordinate) const;
	V2_int tile_size{};
	V2_int tile_count{};
	AABB info{};
	std::vector<ecs::Entity> grid;
};

// Used for comparing existing chunks to newly loaded ones.
inline bool operator==(const Chunk& a, const Chunk& b) { return a.info == b.info && a.tile_size == b.tile_size; }

} // namespace engine