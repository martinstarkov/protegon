#include "ChunkManager.h"

#include <unordered_set>
#include <algorithm>

#include "components/TransformComponent.h"
#include "components/HitboxComponent.h"
#include "components/ShapeComponent.h"
#include "physics/collision/AABBvsAABB.h"
#include "physics/collision/Collision.h"

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

void ChunkManager::CenterOn(const V2_double& position) {
	position_ = position;
}

std::vector<Chunk*> ChunkManager::GetChunksBetween(V2_double position1,
												   V2_double position2) {
	std::vector<Chunk*> chunks;
	auto coordinate1 = math::Floor(position1 / chunk_size_);
	auto coordinate2 = math::Floor(position2 / chunk_size_);
	// Ensure that coordinate2 is the bigger coordinate.
	if (coordinate2.x < coordinate1.x) {
		std::swap(coordinate2.x, coordinate1.x);
	}
	if (coordinate2.y < coordinate1.y) {
		std::swap(coordinate2.y, coordinate1.y);
	}
	coordinate1 -= 1;
	coordinate2 += 1;
	// Only one chunk exists
	//if (coordinate1 == coordinate2) {
	//	auto it = loaded_chunks_.find(coordinate1);
	//	// Add chunk to vector if it exists in the loaded chunks.
	//	if (it != std::end(loaded_chunks_)) {
	//		chunks.emplace_back(it->second);
	//	}
	//} else {
		auto coordinate_extent = coordinate2 - coordinate1 + V2_int{ 1, 1 };
		auto chunk_count = coordinate_extent.x * coordinate_extent.y;
		assert(chunk_count > 1 &&
			   "Math mistake in retrieving chunks between two separate chunk coordinates");
		chunks.reserve(chunk_count);
		for (auto i = coordinate1.x; i < coordinate2.x + 1; ++i) {
			for (auto j = coordinate1.y; j < coordinate2.y + 1; ++j) {
				auto it = loaded_chunks_.find(V2_int{ i, j });
				// Add chunk to vector if it exists in the loaded chunks.
				if (it != std::end(loaded_chunks_)) {
					chunks.emplace_back(it->second);
				}
			}
		}
	//}
	return chunks;
}

void ChunkManager::ResolveCollisionsWith(ecs::Entity& entity) {
	auto& transform = entity.GetComponent<ptgn::TransformComponent>();
	auto& hitbox = entity.GetComponent<ptgn::HitboxComponent>();
	auto& shape = entity.GetComponent<ptgn::ShapeComponent>();
	auto center = shape.shape->GetCenter(transform.transform.position);
	auto size = shape.GetSize();
	auto top_left = center - size / 2.0;
	auto bottom_right = center + size / 2.0;
	auto chunks = GetChunksBetween(top_left, bottom_right);
	for (auto chunk : chunks) {
		chunk->GetManager().ForEachEntityWith<ptgn::TransformComponent, ptgn::HitboxComponent, ptgn::ShapeComponent>(
			[&](ecs::Entity entity2,
				ptgn::TransformComponent& transform2,
				ptgn::HitboxComponent& hitbox2,
				ptgn::ShapeComponent& shape2) {
			ResolveCollision(entity, entity2, transform, transform2, hitbox, hitbox2, shape, shape2);
		});
	}
}

void ChunkManager::Update() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		if (chunk->update_) {
			chunk->Update();
		}
	}
}

void ChunkManager::Render() {
	for (auto [coordinate, chunk] : loaded_chunks_) {
		if (chunk->render_) {
			chunk->Render();
		}
	}
}

const Chunk& ChunkManager::GetChunk(const V2_int& coordinate) const {
	auto it = loaded_chunks_.find(coordinate);
	assert(it != std::end(loaded_chunks_) &&
		   "Cannot GetChunk whose coordinate does not exist in the ChunkManager");
	return *it->second;
}

Chunk& ChunkManager::GetChunk(const V2_int& coordinate) {
	return const_cast<Chunk&>(static_cast<const ChunkManager&>(*this).GetChunk(coordinate));
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