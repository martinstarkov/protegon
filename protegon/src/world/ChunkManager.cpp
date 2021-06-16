#include "ChunkManager.h"

#include "physics/collision/AABBvsAABB.h"

// TODO: Remove
#include "debugging/Debug.h"
#include "event/InputHandler.h"

#include <unordered_set>
#include <algorithm>

namespace ptgn {

ChunkManager::ChunkManager(const V2_int& tiles_per_chunk, const V2_int& tile_size) :
	tiles_per_chunk_{ tiles_per_chunk },
	tile_size_{ tile_size },
	chunk_size_{ tiles_per_chunk_ * tile_size_ } {
	assert(loaded_chunks_.size() == 0 && "Chunk manager chunks cannot be initialized beforehand");
	for (auto i = coordinate_.x; i < coordinate_.x + size_.x; ++i) {
		for (auto j = coordinate_.y; j < coordinate_.y + size_.y; ++j) {
			V2_int add_coordinate{ i, j };
			auto add_it = loaded_chunks_.find(add_coordinate);
			if (add_it == loaded_chunks_.end()) {
				auto chunk = new BasicChunk();
				chunk->Create(add_coordinate, tiles_per_chunk_, tile_size_);
				loaded_chunks_.emplace(add_coordinate, chunk);
			}
		}
	}
}

ChunkManager::~ChunkManager() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		delete chunk;
	}
}

void ChunkManager::Update() {

	Timer timer;
	timer.Start();

	bool chunk_change = false;
	AABB boundary{ size_ };
	AABB chunk_boundary{ { 1, 1 } };
	int erased = 0;

	for (auto it = std::begin(loaded_chunks_); it != std::end(loaded_chunks_);) {
		if (!math::AABBvsAABB(boundary, coordinate_, chunk_boundary, it->first)) {
			// Range check here in future?
			++erased;
			delete it->second;
			it = loaded_chunks_.erase(it); // previously this was something like m_map.erase(it++);
		} else {
			++it;
		}
	}
	int added = 0;
	for (auto i = coordinate_.x; i < coordinate_.x + size_.x; ++i) {
		for (auto j = coordinate_.y; j < coordinate_.y + size_.y; ++j) {
			V2_int potential_coordinate{ i, j };
			auto it = loaded_chunks_.find(potential_coordinate);
			if (it == std::end(loaded_chunks_)) {
				++added;
				auto chunk = new BasicChunk();
				chunk->Create(potential_coordinate, tiles_per_chunk_, tile_size_);
				loaded_chunks_.emplace(potential_coordinate, chunk);
			}
		}
	}

	if (erased > 0 || added > 0) {
		PrintLine("Erased: ", erased, ", added: ", added);
		chunk_change = true;
	}

	//DebugRenderer<WorldRenderer>::DrawRectangle(coordinate_ * chunk_size_, size_ * chunk_size_, colors::ORANGE);

	if (chunk_change) {
		PrintLine(timer.Elapsed<milliseconds>().count());
	}
}

void ChunkManager::CenterOn(const V2_int& position, const V2_int& view_distance) {
	coordinate_ = math::Round(static_cast<V2_double>(position - view_distance) / chunk_size_);
	size_ = math::Round((view_distance * 2.0) / chunk_size_);
}

void ChunkManager::Render() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		chunk->Render();
	}
}

} // namespace ptgn