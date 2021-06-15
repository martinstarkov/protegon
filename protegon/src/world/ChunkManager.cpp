#include "ChunkManager.h"

#include "physics/collision/AABBvsAABB.h"

// TODO: Remove
#include "debugging/Debug.h"

namespace ptgn {

ChunkManager::ChunkManager(const V2_int& tiles_per_chunk, const V2_int& tile_size) :
	tiles_per_chunk_{ tiles_per_chunk },
	tile_size_{ tile_size },
	chunk_size_{ tiles_per_chunk_ * tile_size_ } {
	for (auto i{ 0 }; i < 20; ++i) {
		for (auto j{ 0 }; j < 20; ++j) {
			auto chunk{ new BasicChunk() };
			chunk->Create(V2_int{ i, j } * chunk_size_, tiles_per_chunk_, tile_size_);
			loaded_chunks_.emplace_back(chunk);
		}
	}

}

ChunkManager::~ChunkManager() {
	for (auto chunk : loaded_chunks_) {
		delete chunk;
	}
}

void ChunkManager::SetRenderRadius(const V2_int& chunk_radius) {
	render_size_ = chunk_radius * 2;
}

void ChunkManager::SetUpdateRadius(const V2_int& chunk_radius) {
	update_size_ = chunk_radius * 2;
}

void ChunkManager::Update() {
	auto offset_position{ previous_position_ - active_position_ };
	auto offset_size{ previous_size_ - active_size_ };
	auto changed_position{ !offset_position.IsZero() };
	auto changed_size{ !offset_size.IsZero() };

	for (auto chunk : loaded_chunks_) {
		chunk->flagged_ = false;
	}

	AABB boundary{ active_size_ };
	AABB chunk_shape{ chunk_size_ };
	AllocationMetrics::PrintMemoryUsage();
	render_chunks_.clear();
	for (auto chunk : loaded_chunks_) {
		bool colliding = math::AABBvsAABB(boundary, active_position_, chunk_shape, chunk->position_);
		Color color = colors::BLACK;
		if (colliding) {
			color = colors::RED;
		}
		DebugRenderer<WorldRenderer>::DrawRectangle(chunk->position_, chunk_shape.size, color);
		if (colliding) {
			render_chunks_.emplace_back(chunk);
		}
	}

	previous_position_ = active_position_;
	previous_size_ = active_size_;
}

void ChunkManager::CenterOn(const V2_int& position, const V2_int& chunk_radius) {
	auto active_radius = chunk_radius * chunk_size_;
	active_position_ = position - active_radius;
	active_size_ = active_radius * 2;
}

void ChunkManager::Render() {
	for (auto chunk : render_chunks_) {
		chunk->Render();
	}
}

} // namespace ptgn









//
//void ChunkManager::Update(ecs::Entity& player, const Camera& camera) {
//
//	auto [transform, shape] = player.GetComponents<TransformComponent, ShapeComponent>();//, CollisionComponent>();
//	auto player_size{ shape.GetSize() };
//
//
//
//
//
//
//	/*if (player_chunks_.size() > 0) {
//		using CollisionTuple = std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>;
//		using CollisionContainer = std::vector<CollisionTuple>;
//		CollisionContainer chunk_entities;
//		chunk_entities.reserve(player_chunks_.size() * tiles_per_chunk_.x * tiles_per_chunk_.y);
//		for (auto chunk : player_chunks_) {
//			auto entities = chunk->GetManager().GetEntityComponents<TransformComponent, CollisionComponent>();
//			chunk_entities.insert(chunk_entities.end(), entities.begin(), entities.end());
//		}
//		CollisionContainer players;
//		players.emplace_back(player, transform, collision);
//		auto collisions{ CollisionRoutine<int>(players, chunk_entities) };
//		for (auto [entity, collision] : collisions) {
//			mine(entity, collision);
//		}
//		world_manager.Refresh();
//		for (auto chunk : player_chunks_) {
//			chunk->GetManager().Refresh();
//		}
//	} else {
//		player.GetManager().UpdateSystem<CollisionSystem>();
//	}*/
//
//
//
//
//	V2_double chunk_size{ tiles_per_chunk_ * tile_size_ };
//	V2_double lowest{ math::Floor(camera.position / chunk_size) };
//	V2_double highest{
//		math::Ceil((camera.position + static_cast<V2_double>(Window::GetSize()) / camera.scale) / chunk_size)
//	};
//	// Optional: Expand loaded chunk region.
//	//lowest += -1;
//	//highest += 1;
//	assert(lowest.x <= highest.x &&
//		   "Left grid edge cannot be above right grid edge");
//	assert(lowest.y <= highest.y &&
//		   "Top grid edge cannot be below bottom grid edge");
//
//	auto boundary_position = lowest * chunk_size;
//	auto boundary_size = math::Abs(highest - lowest) * chunk_size;
//
//	V2_int player_lowest{ V2_int::Maximum() };
//	V2_int player_highest{ -V2_int::Maximum() };
//	// Find all the chunks which contain the player.
//
//	auto min_chunk_pos{ math::Floor(transform.transform.position / chunk_size) };
//	auto max_chunk_pos{ math::Floor((transform.transform.position + player_size) / chunk_size) };
//	player_lowest = math::Min(player_lowest, min_chunk_pos);
//	player_highest = math::Max(player_highest, max_chunk_pos);
//
//	// Extend loaded player chunk range by this amount to each side.
//	auto additional_chunk_range{ 1 };
//	player_lowest += -additional_chunk_range;
//	player_highest += additional_chunk_range;
//
//	assert(player_lowest.x <= player_highest.x && "Left player chunk edge cannot be above right player chunk edge");
//	assert(player_lowest.y <= player_highest.y && "Top player chunk edge cannot be above bottom player chunk edge");
//
//	//auto player_chunks_box = AABB{ player_lowest * chunk_size, Abs(player_highest - player_lowest) * chunk_size };
//
//	// Remove old chunks that have gone out of range of camera.
//	auto it{ world_chunks_.begin() };
//	while (it != world_chunks_.end()) {
//		auto chunk{ *it };
//		auto chunk_size{ chunk->size_ };
//		auto chunk_position{ chunk->position_ };
//		chunk_size *= tile_size_;
//		if (!chunk ||
//			!math::AABBvsAABB(AABB{ chunk_size }, chunk_position, AABB{ boundary_size }, boundary_position)) {
//			delete chunk;
//			it = world_chunks_.erase(it);
//		} else {
//			++it;
//		}
//	}
//
//
//	player_chunks_.clear();
//	// Go through all new chunks.
//	for (auto i{ lowest.x }; i != highest.x; ++i) {
//		for (auto j{ lowest.y }; j != highest.y; ++j) {
//			// AABB corresponding to the potentially new chunk.
//			V2_double grid_position{ i, j };
//			auto potential_chunk_position = chunk_size * grid_position;
//			bool new_chunk{ true };
//			bool player_chunk{ false };
//			if (i >= player_lowest.x && i <= player_highest.x && j >= player_lowest.y && j <= player_highest.y) {
//				player_chunk = true;
//			}
//			for (auto c : world_chunks_) {
//				if (c) {
//					// Check if chunk exists already, if so, skip it.
//					if (c->position_ == potential_chunk_position) {
//						c->SetNewChunk(false);
//						new_chunk = false;
//						if (player_chunk) {
//							player_chunks_.emplace_back(c);
//						}
//						break;
//					}
//				}
//			}
//			if (new_chunk) {
//				auto chunk = new Chunk();
//				chunk->Init(potential_chunk_position, tiles_per_chunk_, tile_size_);
//				chunk->InitBackground(noise);
//				chunk->Generate(noise);
//				chunk->SetNewChunk(true);
//				world_chunks_.emplace_back(chunk);
//				if (player_chunk) {
//					player_chunks_.emplace_back(chunk);
//				}
//			}
//		}
//	}
//
//
//}