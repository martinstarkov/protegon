#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

using namespace engine;

engine::Particle test_particle{ {}, {}, {}, 0, engine::ORANGE, engine::GREY, 10, 0, 0, 0.15 };
engine::ParticleManager particles{ 300 };

void mine(ecs::Entity& entity, Collision& collision) {
	auto& c = collision.entity.GetComponent<CollisionComponent>();
	test_particle.position = c.collider.Center();
	V2_double scale = { 3.0, 3.0 };
	test_particle.velocity = scale * V2_double{ -1, 0 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ -1, -1 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ 0, -1 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ 1, 0 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ 1, 1 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ 0, 1 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ -1, 1 };
	particles.Emit(test_particle);
	test_particle.velocity = scale * V2_double{ 1, -1 };
	particles.Emit(test_particle);
	collision.entity.Destroy();
}

class Game : public engine::Engine {
public:
	std::vector<engine::Image> images;
	V2_int tiles_per_chunk = { 16, 16 };
	V2_int tile_size = { 32, 32 };
	void Init() {
		auto& scene = Scene::Get();
		LOG("Initializing game systems...");
		scene.manager.AddSystem<RenderSystem>();
		scene.manager.AddSystem<HitboxRenderSystem>();
		scene.manager.AddSystem<PhysicsSystem>();
		scene.manager.AddSystem<TargetSystem>();
		scene.manager.AddSystem<LifetimeSystem>();
		scene.manager.AddSystem<AnimationSystem>();
		scene.manager.AddSystem<CollisionSystem>();
		scene.manager.AddSystem<InputSystem>();
		scene.manager.AddSystem<StateMachineSystem>();
		scene.manager.AddSystem<DirectionSystem>();
		scene.manager.AddSystem<CameraSystem>();
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<StateMachineSystem>();
		scene.ui_manager.AddSystem<UIRenderer>();

		title_screen = scene.event_manager.CreateEntity();
		scene.event_manager.Refresh();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title = title_screen.AddComponent<TitleScreenComponent>();
		title_screen.AddComponent<EventComponent>(scene);
		engine::EventHandler::Invoke(title_screen);
		title.open = true;
		LOG("Initialized all game systems successfully");
	}

	engine::ValueNoise<float> noise{ static_cast<V2_int>(tiles_per_chunk) };

    void Update() {
		auto& scene = Scene::Get();
		auto players = scene.manager.GetEntityComponents<TransformComponent, CollisionComponent, PlayerController>();
		scene.manager.UpdateSystem<InputSystem>();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyDown(Key::R)) {
			engine::EventHandler::Invoke(title_screen);
			title.open = true;
			particles.Reset();
		}
		scene.manager.UpdateSystem<PhysicsSystem>();
		scene.manager.UpdateSystem<TargetSystem>();

		if (scene.player_chunks.size() > 0 && players.size() > 0) {
			std::vector<std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>> player_entities;
			player_entities.reserve(players.size());
			auto play = scene.manager.GetEntityComponents<TransformComponent, CollisionComponent, PlayerController>();
			for (auto [entity, transform, collider, player] : play) {
				player_entities.emplace_back(entity, transform, collider);
			}
			std::vector<std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>> chunk_entities;
			chunk_entities.reserve(scene.player_chunks.size() * tiles_per_chunk.x * tiles_per_chunk.y);
			for (auto chunk : scene.player_chunks) {
				auto entities = chunk->manager.GetEntityComponents<TransformComponent, CollisionComponent>();
				chunk_entities.insert(chunk_entities.end(), entities.begin(), entities.end());
			}
			CollisionRoutine(player_entities, chunk_entities, &mine);
			scene.manager.Refresh();
			for (auto chunk : scene.player_chunks) {
				chunk->manager.Refresh();
			}
		} else {
			scene.manager.UpdateSystem<CollisionSystem>();
		}

		scene.manager.UpdateSystem<StateMachineSystem>();
		scene.ui_manager.UpdateSystem<StateMachineSystem>();
		scene.manager.UpdateSystem<DirectionSystem>();
		scene.manager.UpdateSystem<CameraSystem>();

		auto& title_ = title_screen.GetComponent<TitleScreenComponent>();
		if (title_.open && scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
			title_.open = false;
		}

		auto camera = scene.GetCamera();
		players = scene.manager.GetEntityComponents<TransformComponent, CollisionComponent, PlayerController>();
		if (camera && !title.open && players.size() > 0) {

			V2_double chunk_size = tiles_per_chunk * tile_size;
			V2_double lowest = Floor(camera->offset / chunk_size);
			V2_double highest = Ceil((camera->offset + static_cast<V2_double>(engine::Engine::GetScreenSize()) / camera->scale) / chunk_size);
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
				auto min_chunk_pos = Floor<double>(transform.position / chunk_size);
				auto max_chunk_pos = Floor<double>((transform.position + collision.collider.size) / chunk_size);
				player_lowest.x = std::min(player_lowest.x, min_chunk_pos.x);
				player_lowest.y = std::min(player_lowest.y, min_chunk_pos.y);
				player_highest.x = std::max(player_highest.x, max_chunk_pos.x);
				player_highest.y = std::max(player_highest.y, max_chunk_pos.y);
			}

			//LOG("Player lowest: " << player_lowest << ", Player highest: " << player_highest << ", Players: " << players.size());

			// Extend loaded player chunk range by this amount to each side.
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
						chunk->Generate(noise);
						chunk->new_chunk = true;
						scene.chunks.emplace_back(chunk);
						if (player_chunk) {
							scene.player_chunks.emplace_back(chunk);
						}
					}
				}
			}
		} else {
			scene.chunks.clear();
		}

		particles.Update();
    }
	void Render() {
		auto& scene = Scene::Get();

		scene.manager.UpdateSystem<AnimationSystem>();

		// TODO: Consider a better way of doing this?
		for (auto& c : scene.chunks) {
			c->manager.UpdateSystem<TileRenderSystem>();
		}

		scene.manager.UpdateSystem<RenderSystem>();
		scene.manager.UpdateSystem<HitboxRenderSystem>();

		scene.ui_manager.UpdateSystem<UIRenderer>();
		particles.Render();
	}
private:
	ecs::Entity title_screen;
};

int main(int argc, char* args[]) { // sdl main override

	Engine::Start<Game>("Protegon", 512 * 2, 600, 60);

    return 0;
}