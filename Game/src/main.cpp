#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

class MyGame : public engine::Engine {
public:
	std::vector<engine::Image> images;
	V2_int tiles_per_chunk = { 16, 16 };
	V2_int tile_size = { 32, 32 };
	//V2_int tiles_per_chunk = { 8, 8 };
	//V2_int tile_size = { 64, 64 };
	void Init() {

		LOG("Initializing game systems...");
		scene.manager.AddSystem<RenderSystem>(&scene);
		scene.manager.AddSystem<HitboxRenderSystem>(&scene);
		scene.manager.AddSystem<PhysicsSystem>();
		scene.manager.AddSystem<TargetSystem>();
		scene.manager.AddSystem<LifetimeSystem>();
		scene.manager.AddSystem<AnimationSystem>();
		scene.manager.AddSystem<CollisionSystem>();
		scene.manager.AddSystem<InputSystem>();
		scene.manager.AddSystem<StateMachineSystem>();
		scene.manager.AddSystem<DirectionSystem>();
		scene.manager.AddSystem<CameraSystem>(&scene);
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<UIButtonListener>(&scene);
		scene.ui_manager.AddSystem<UIButtonRenderer>();
		scene.ui_manager.AddSystem<UITextRenderer>();

		//LOG("Sectors: " << Engine::ScreenSize() / tile_size);

		title_screen = scene.event_manager.CreateEntity();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title = title_screen.AddComponent<TitleScreenComponent>();
		engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
		title.open = true;
		LOG("Initialized all game systems successfully");
		
	}

	int octave = 5;
	double bias = 2.0;

    void Update() {
		auto players = scene.manager.GetComponentTuple<TransformComponent, CollisionComponent, PlayerController>();
		scene.manager.Update<InputSystem>();
		scene.ui_manager.Update<UIButtonListener>();
		scene.manager.Update<PhysicsSystem>();
		scene.manager.Update<TargetSystem>();
		/*if (scene.player_chunks.size() > 0 && players.size() > 0) {
			std::vector<std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>> chunk_entities;
			chunk_entities.reserve(tiles_per_chunk.x * tiles_per_chunk.y * scene.player_chunks.size() + players.size());
			for (auto chunk : scene.player_chunks) {
				auto entities = chunk->manager.GetComponentTuple<TransformComponent, CollisionComponent>();
				chunk_entities.insert(chunk_entities.end(), entities.begin(), entities.end());
			}
			for (auto [entity, transform, collider, player] : players) {
				chunk_entities.emplace_back(entity, transform, collider);
			}
			CollisionRoutine(chunk_entities);
		} else {
			scene.manager.Update<CollisionSystem>();
		}*/
		auto entities = scene.manager.GetComponentTuple<TransformComponent, CollisionComponent>();
		for (auto [entity, transform, collider] : entities) {
			for (auto [entity2, transform2, collider2] : entities) {
				if (entity != entity2) {

				}
			}
		}

		scene.manager.Update<StateMachineSystem>();
		scene.manager.Update<DirectionSystem>();
		//scene.manager.Update<LifetimeSystem>();
		scene.manager.Update<CameraSystem>();
		//AllocationMetrics::PrintMemoryUsage();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyDown(Key::R)) {
			engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
			title.open = true;
		} else if (title.open) {
			if (scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
				title.open = false;
			}
		}

		auto camera = scene.GetCamera();
		if (camera && !title.open && players.size() > 0) {

			V2_double chunk_size = tiles_per_chunk * tile_size;
			V2_double lowest = Floor(camera->offset / chunk_size);
			V2_double highest = Ceil((camera->offset + static_cast<V2_double>(engine::Engine::ScreenSize()) / camera->scale) / chunk_size);
			// Optional: Expand loaded chunk region.
			//lowest += -1;
			//highest += 1;
			assert(lowest.x <= highest.x && "Left grid edge cannot be above right grid edge");
			assert(lowest.y <= highest.y && "Top grid edge cannot be below bottom grid edge");

			auto camera_box = AABB{ lowest * chunk_size, Abs(highest - lowest) * chunk_size };

			V2_double player_lowest = V2_double::Infinite();
			V2_double player_highest = -V2_double::Infinite();
			// Find all the chunks which contain the player.
			for (auto [entity, transform, collision, player] : players) {
				auto min_chunk_pos = Floor(transform.position / chunk_size);
				auto max_chunk_pos = Floor((transform.position + collision.collider.size) / chunk_size);
				player_lowest.x = std::min(player_lowest.x, min_chunk_pos.x);
				player_lowest.y = std::min(player_lowest.y, min_chunk_pos.y);
				player_highest.x = std::max(player_highest.x, max_chunk_pos.x);
				player_highest.y = std::max(player_highest.y, max_chunk_pos.y);
			}

			//LOG("Player lowest: " << player_lowest << ", Player highest: " << player_highest << ", Players: " << players.size());

			// Extend chunk range.
			auto additional_chunk_range = 1;
			player_lowest += -additional_chunk_range;
			player_highest += additional_chunk_range;

			assert(player_lowest.x <= player_highest.x && "Left player chunk edge cannot be above right player chunk edge");
			assert(player_lowest.y <= player_highest.y && "Top player chunk edge cannot be above bottom player chunk edge");

			//auto player_chunks_box = AABB{ player_lowest * chunk_size, Abs(player_highest - player_lowest) * chunk_size };

			// Remove old chunks that have gone out of range of camera.
			auto it = scene.chunks.begin();
			while (it != scene.chunks.end()) {
				auto chunk = (*it);
				auto chunk_box = chunk->GetInfo();
				chunk_box.size *= tile_size;
				if (!chunk || !engine::collision::AABBvsAABB(chunk_box, camera_box)) {
					delete chunk;
					it = scene.chunks.erase(it);
				} else {
					++it;
				}
			}

			scene.player_chunks.clear();
			// Go through all new chunks.
			for (auto i = lowest.x; i != highest.x; ++i) {
				for (auto j = lowest.y; j != highest.y; ++j) {
					// AABB corresponding to the potentially new chunk.
					auto grid_position = V2_double{ i, j };
					auto potential_chunk = AABB{ chunk_size * grid_position, tiles_per_chunk };
					bool new_chunk = true;
					bool player_chunk = false;
					if (i >= player_lowest.x && i <= player_highest.x && j >= player_lowest.y && j <= player_highest.y) {
						player_chunk = true;
					}
					for (auto c : scene.chunks) {
						if (c) {
							// Check if chunk exists already, if so, skip it.
							if (c->GetInfo() == potential_chunk) {
								c->new_chunk = false;
								new_chunk = false;
								if (player_chunk) {
									scene.player_chunks.emplace_back(c);
								}
								break;
							}
						}
					}
					if (new_chunk) {
						auto chunk = new BoxChunk();
						chunk->Init(potential_chunk, tile_size, &scene);
						engine::Timer timer;
						timer.Start();
						chunk->Generate(0, octave, bias);
						chunk->new_chunk = true;
						scene.chunks.push_back(chunk);
						if (player_chunk) {
							scene.player_chunks.emplace_back(chunk);
						}
					}
				}
			}
			//LOG("Chunks: " << chunks.size());
		} else {
			for (auto c : scene.chunks) {
				if (c) {
					delete c;
					c = nullptr;
				}
			}
			scene.chunks.clear();
		}
		//LOG("Octave: " << octave << ", bias: " << bias);
    }
	void Render() {
		scene.manager.Update<AnimationSystem>();
		auto camera = scene.GetCamera();
		if (camera) {
			// TODO: Consider a better way of doing this?
			for (auto& c : scene.chunks) {
				if (c->new_chunk) {
					c->Update();
					c->new_chunk = false;
				}
				auto chunk_box = c->GetInfo();
				chunk_box.size *= tile_size;
				c->Render();
				//engine::TextureManager::DrawRectangle((chunk_box.position - camera->offset) * camera->scale, Ceil(chunk_box.size * camera->scale), engine::PURPLE);
			}
		}
		scene.manager.Update<RenderSystem>();
		scene.manager.Update<HitboxRenderSystem>();

		scene.ui_manager.Update<UIButtonRenderer>();
		scene.ui_manager.Update<UITextRenderer>();
	}
private:
	ecs::Entity title_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Protegon");
	engine::Engine::Start<MyGame>("Protegon", 512 * 2, 600, 60);

    return 0;
}