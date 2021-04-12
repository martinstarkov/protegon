//#include "ChunkManager.h"
//
//#include "math/Functions.h"
//#include "physics/collision/static/AABBvsAABB.h"
//#include "core/Engine.h"
//#include "core/Scene.h"
//#include "core/World.h"
//#include "procedural/Chunk.h"
//
//#include "ecs/components/TransformComponent.h"
//#include "ecs/components/CollisionComponent.h"
//#include "inventory/Inventory.h"
//
//#include "ecs/systems/HitboxRenderSystem.h"
//#include "ecs/systems/CollisionSystem.h"
//
//namespace engine {
//
//ChunkManager::ChunkManager(const V2_int& tile_size, const V2_int& tiles_per_chunk) :
//	tile_size_{ tile_size },
//	tiles_per_chunk_{ tiles_per_chunk },
//	noise{ tiles_per_chunk, 2000 } {}
//
//void ChunkManager::mine(ecs::Entity& entity, Collision& collision) {
//	auto& c = collision.entity.GetComponent<CollisionComponent>();
//	test_particle.position = c.collider.Center();
//	V2_double scale = { 3.0, 3.0 };
//	test_particle.velocity = scale * V2_double{ -1, 0 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ -1, -1 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ 0, -1 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ 1, 0 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ 1, 1 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ 0, 1 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ -1, 1 };
//	particles.Emit(test_particle);
//	test_particle.velocity = scale * V2_double{ 1, -1 };
//	particles.Emit(test_particle);
//	if (entity.HasComponent<InventoryComponent>()) {
//		auto& inventory{ entity.GetComponent<InventoryComponent>() };
//		auto& material{ collision.entity.GetComponent<MaterialComponent>() };
//		inventory.AddTile(material.type);
//		//LOG("Inventory: (" << inventory.GetTileSlot(Material::M_IRON).count << ", " << inventory.GetTileSlot(Material::M_IRON).texture_key << "), (" << inventory.GetTileSlot(Material::M_SILVER).count << ", " << inventory.GetTileSlot(Material::M_SILVER).texture_key << ")");
//	}
//	collision.entity.Destroy();
//}
//
//void ChunkManager::Update() {
//	auto& scene{ Scene::Get() };
//	auto& world{ *scene.world };
//	auto& world_manager{ world.GetManager() };
//	auto player{ world.GetPlayer() };
//
//	auto [transform, collision] = player.GetComponents<TransformComponent, CollisionComponent>();
//
//
//
//
//
//
//
//	if (player_chunks_.size() > 0) {
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
//	}
//
//
//
//
//
//
//
//	auto camera{ scene.GetCamera() };
//	assert(camera != nullptr && "Active scene camera must exist");
//	V2_double chunk_size{ tiles_per_chunk_ * tile_size_ };
//	V2_double lowest{ Floor(camera->offset / chunk_size) };
//	V2_double highest{ Ceil((camera->offset + static_cast<V2_double>(engine::Engine::GetScreenSize()) / camera->scale) / chunk_size) };
//	// Optional: Expand loaded chunk region.
//	//lowest += -1;
//	//highest += 1;
//	assert(lowest.x <= highest.x && "Left grid edge cannot be above right grid edge");
//	assert(lowest.y <= highest.y && "Top grid edge cannot be below bottom grid edge");
//
//	AABB camera_box{ lowest * chunk_size, Abs(highest - lowest) * chunk_size };
//
//	V2_int player_lowest{ V2_int::Maximum() };
//	V2_int player_highest{ -V2_int::Maximum() };
//	// Find all the chunks which contain the player.
//
//	auto min_chunk_pos{ Floor(transform.position / chunk_size) };
//	auto max_chunk_pos{ Floor((transform.position + collision.collider.size) / chunk_size) };
//	player_lowest.x = std::min(player_lowest.x, min_chunk_pos.x);
//	player_lowest.y = std::min(player_lowest.y, min_chunk_pos.y);
//	player_highest.x = std::max(player_highest.x, max_chunk_pos.x);
//	player_highest.y = std::max(player_highest.y, max_chunk_pos.y);
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
//		auto chunk_box{ chunk->GetInfo() };
//		chunk_box.size *= tile_size_;
//		if (!chunk || !engine::collision::AABBvsAABB(chunk_box, camera_box)) {
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
//			AABB potential_chunk{ chunk_size * grid_position, tiles_per_chunk_ };
//			bool new_chunk{ true };
//			bool player_chunk{ false };
//			if (i >= player_lowest.x && i <= player_highest.x && j >= player_lowest.y && j <= player_highest.y) {
//				player_chunk = true;
//			}
//			for (auto c : world_chunks_) {
//				if (c) {
//					// Check if chunk exists already, if so, skip it.
//					if (c->GetInfo() == potential_chunk) {
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
//				auto chunk = world.MakeChunk();
//				chunk->Init(potential_chunk, tile_size_);
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
//	particles.Update();
//}
//
//void ChunkManager::Reset() {
//	particles.Reset();
//}
//
//void ChunkManager::Render() {
//	for (auto chunk : world_chunks_) {
//		chunk->RenderBackground();
//	}
//	// TODO: Consider a better way of doing this?
//	for (auto chunk : world_chunks_) {
//		chunk->GetManager().UpdateSystem<TileRenderSystem>();
//	}
//	particles.Render();
//}
//
//void ChunkManager::Clear() {
//	world_chunks_.clear();
//	player_chunks_.clear();
//}
//
//} // namespace engine