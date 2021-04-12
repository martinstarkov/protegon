//#pragma once
//
//#include <vector> // std::vector
//#include <cstdlib> // std::size_t
//
//#include "ecs/ECS.h" // ecs::Entity, ecs::Manager
//#include "math/Vector2.h"
//#include "math/Noise.h"
//#include "renderer/AABB.h"
//#include "renderer/Texture.h"
//
//namespace engine {
//
//class BaseChunk {
//public:
//	virtual ~BaseChunk() = default;
//	virtual const ecs::Entity& GetEntity(const V2_int& relative_coordinate) const = 0;
//	virtual ecs::Entity& GetEntity(const V2_int& relative_coordinate) = 0;
//	virtual const AABB& GetInfo() const = 0;
//	virtual void Unload() = 0;
//	virtual void Init(const AABB& chunk_info, const V2_int& tile_size) = 0;
//	virtual void InitBackground(const ValueNoise<float>& noise) = 0;
//	virtual void RenderBackground() = 0;
//	virtual void Generate(const ValueNoise<float>& noise) = 0;
//	virtual void SetNewChunk(bool state) = 0;
//	virtual ecs::Manager& GetManager() = 0;
//	virtual void Update() = 0;
//};
//
//class Chunk : public BaseChunk {
//public:
//	virtual ~Chunk() override;
//	virtual const ecs::Entity& GetEntity(const V2_int& relative_coordinate) const override final;
//	virtual ecs::Entity& GetEntity(const V2_int& relative_coordinate) override final;
//	virtual const AABB& GetInfo() const override final;
//	// Destroys all grid entities.
//	virtual void Unload() override final;
//	virtual void Init(const AABB& chunk_info, const V2_int& tile_size) override final;
//	virtual void InitBackground(const ValueNoise<float>& noise) override {}
//	virtual void Update() override {}
//	virtual void SetNewChunk(bool state) override final {
//		new_chunk_ = state;
//	}
//	virtual ecs::Manager& GetManager() override final {
//		return manager_;
//	}
//	friend bool operator==(const Chunk& a, const Chunk& b);
//	virtual void RenderBackground() override final;
//protected:
//	Texture background_texture_;
//	ecs::Manager manager_;
//	std::size_t GetIndex(const V2_int& relative_coordinate) const;
//	V2_int tile_size_;
//	V2_int tile_count_;
//	// info_.position is the top left pixel where the chunk starts.
//	// info_.position / tile_size is the tile position
//	// info_.position / info_.size is the chunk's chunk coordinate (e.g. (0, 1)).
//	// info_.size is the number of tiles inside a chunk.
//	AABB info_;
//	std::vector<ecs::Entity> grid_;
//	bool new_chunk_{ true };
//};
//
//// Used for comparing existing chunks to newly loaded ones.
//inline bool operator==(const Chunk& a, const Chunk& b) { return a.info_ == b.info_ && a.tile_size_ == b.tile_size_; }
//
//} // namespace engine