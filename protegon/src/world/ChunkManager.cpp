#include "ChunkManager.h"

#include "physics/collision/AABBvsAABB.h"

// TODO: Remove
#include "debugging/Debug.h"
#include "event/InputHandler.h"

#include <unordered_set>
#include <algorithm>

namespace ptgn {

ChunkManager::ChunkManager(const V2_int& tiles_per_chunk,
						   const V2_int& tile_size,
						   const V2_int& load_size,
						   const V2_int& update_size,
						   const V2_int& render_size) :
	tiles_per_chunk_{ tiles_per_chunk },
	tile_size_{ tile_size },
	chunk_size_{ tiles_per_chunk_ * tile_size_ },
	load_size_{ load_size },
	update_size_{ update_size },
	render_size_{ render_size } {
}

ChunkManager::~ChunkManager() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		delete chunk;
	}
}

void ChunkManager::Update() {
	Timer timer;
	timer.Start();
	
	auto coordinate = math::Floor(position_ / chunk_size_ - load_size_ / 2);
	bool chunk_change = false;
	AABB boundary{ load_size_ };
	AABB chunk_boundary{ { 1, 1 } };
	int erased = 0;

	for (auto it = std::begin(loaded_chunks_); it != std::end(loaded_chunks_);) {
		if (!math::AABBvsAABB(boundary, coordinate, chunk_boundary, it->first)) {
			// Range check here in future?
			++erased;
			delete it->second;
			it = loaded_chunks_.erase(it); // previously this was something like m_map.erase(it++);
		} else {
			++it;
		}
	}
	int added = 0;
	for (auto i = coordinate.x; i < coordinate.x + load_size_.x; ++i) {
		for (auto j = coordinate.y; j < coordinate.y + load_size_.y; ++j) {
			V2_int potential_coordinate{ i, j };
			auto it = loaded_chunks_.find(potential_coordinate);
			if (it == std::end(loaded_chunks_)) {
				++added;
				auto chunk = new BasicChunk();
				chunk->Init(this, potential_coordinate);
				chunk->Create();
				loaded_chunks_.emplace(potential_coordinate, chunk);
			}
		}
	}

	if (erased > 0 || added > 0) {
		//PrintLine("Erased: ", erased, ", added: ", added);
		chunk_change = true;
	}

	//DebugRenderer<WorldRenderer>::DrawRectangle(coordinate_ * chunk_size_, size_ * chunk_size_, colors::ORANGE);

	if (chunk_change) {
		//PrintLine(timer.Elapsed<milliseconds>().count());
	}
}

void ChunkManager::CenterOn(const V2_double& position) {
	position_ = position;
}

void ChunkManager::Render() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		chunk->Render();
	}
}

V2_int ChunkManager::GetTileSize() const {
	return tile_size_;
}

V2_int ChunkManager::GetTilesPerChunk() const {
	return tiles_per_chunk_;
}

V2_int ChunkManager::GetChunkSize() const {
	return chunk_size_;
}

} // namespace ptgn