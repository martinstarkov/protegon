#pragma once

#include <unordered_map> // std::unordered_map
#include <vector> // std::vector

#include "core/ECS.h"
#include "math/Vector2.h"
#include "world/Chunk.h"

namespace ptgn {

class ChunkManager {
public:
	ChunkManager() = delete;
	~ChunkManager();

	ChunkManager(const V2_int& tiles_per_chunk,
				 const V2_int& tile_size,
				 const V2_int& load_size,
				 const V2_int& update_size,
				 const V2_int& render_size);
	void CenterOn(const V2_double& position);

	std::vector<Chunk*> GetChunksBetween(V2_double position1,
										 V2_double position2);

	template <typename TChunk,
		type_traits::is_base_of_e<Chunk, TChunk> = true>
	void UpdateChunks() {
		auto load_coordinate = math::Floor(position_ / chunk_size_ - load_size_ / 2);
		auto update_coordinate = math::Floor(position_ / chunk_size_ - update_size_ / 2);
		auto render_coordinate = math::Floor(position_ / chunk_size_ - render_size_ / 2);
		bool chunk_change = false;
		AABB load_boundary{ load_size_ };
		AABB update_boundary{ update_size_ };
		AABB render_boundary{ render_size_ };
		AABB chunk_boundary{ { 1, 1 } };
		int erased = 0;
		for (auto it = std::begin(loaded_chunks_); it != std::end(loaded_chunks_);) {
			auto chunk_coordinate{ it->first };
			auto chunk{ it->second };
			chunk->update_ = math::AABBvsAABB(update_boundary, update_coordinate, chunk_boundary, chunk_coordinate);
			chunk->render_ = math::AABBvsAABB(render_boundary, render_coordinate, chunk_boundary, chunk_coordinate);
			if (!math::AABBvsAABB(load_boundary, load_coordinate, chunk_boundary, chunk_coordinate)) {
				// Range check here in future?
				++erased;
				delete chunk;
				it = loaded_chunks_.erase(it);
			} else {
				++it;
			}
		}
		int added = 0;
		for (auto i = load_coordinate.x; i < load_coordinate.x + load_size_.x; ++i) {
			for (auto j = load_coordinate.y; j < load_coordinate.y + load_size_.y; ++j) {
				V2_int potential_coordinate{ i, j };
				auto it = loaded_chunks_.find(potential_coordinate);
				if (it == std::end(loaded_chunks_)) {
					++added;
					auto chunk{ new TChunk{} };
					chunk->Init(this, potential_coordinate);
					chunk->Create();
					loaded_chunks_.emplace(potential_coordinate, chunk);
				}
			}
		}
	}

	void ResolveCollisionsWith(ecs::Entity& entity);

	void Update();

	void Render();

	const Chunk& GetChunk(const V2_int& coordinate) const;

	Chunk& GetChunk(const V2_int& coordinate);

	V2_int GetTileSize() const;

	V2_int GetTilesPerChunk() const;

	V2_int GetChunkSize() const;
private:

	V2_int tiles_per_chunk_;
	V2_int tile_size_;
	
	V2_double position_;
	V2_double chunk_size_;

	V2_int load_size_;
	V2_int update_size_;
	V2_int render_size_;

	std::unordered_map<V2_int, Chunk*> loaded_chunks_;
};

} // namespace ptgn