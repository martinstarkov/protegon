#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

using namespace engine;

engine::Particle test_particle{ {}, {}, {}, 0, engine::ORANGE, engine::GREY, 10, 0, 0, 0.3 };
engine::ParticleManager particles{ 400 };

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
	particles.Refresh();
	collision.entity.Destroy();
	collision.entity.GetManager().Refresh();
}

class Game : public engine::Engine {
public:
	std::vector<engine::Image> images;
	V2_int tiles_per_chunk = { 16, 16 };
	V2_int tile_size = { 32, 32 };
	//V2_int tiles_per_chunk = { 8, 8 };
	//V2_int tile_size = { 64, 64 };
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

		//LOG("Sectors: " << Engine::GetScreenSize() / tile_size);

		title_screen = scene.event_manager.CreateEntity();
		scene.event_manager.Refresh();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title = title_screen.AddComponent<TitleScreenComponent>();
		title_screen.AddComponent<EventComponent>(scene);
		engine::EventHandler::Invoke(title_screen);
		title.open = true;
		LOG("Initialized all game systems successfully");

	}

	int octave = 5;
	double bias = 2.0;

    void Update() {
		auto& scene = Scene::Get();
		Timer timer0;
		timer0.Start();
		auto players = scene.manager.GetComponentTuple<TransformComponent, CollisionComponent, PlayerController>();
		scene.manager.UpdateSystem<InputSystem>();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyDown(Key::R)) {
			engine::EventHandler::Invoke(title_screen);
			title.open = true;
			particles.Reset();
		}
		scene.manager.UpdateSystem<PhysicsSystem>();
		scene.manager.UpdateSystem<TargetSystem>(true);
		//if (timer0.ElapsedMilliseconds() > 1)
		//LOG("timer0: " << timer0.ElapsedMilliseconds());
		Timer timer1;
		timer1.Start();
		if (scene.player_chunks.size() > 0 && players.size() > 0) {
			std::vector<std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>> player_entities;
			player_entities.reserve(players.size());
			auto play = scene.manager.GetComponentTuple<TransformComponent, CollisionComponent, PlayerController>();
			for (auto [entity, transform, collider, player] : play) {
				player_entities.emplace_back(entity, transform, collider);
			}
			std::vector<std::tuple<ecs::Entity, TransformComponent&, CollisionComponent&>> chunk_entities;
			chunk_entities.reserve(scene.player_chunks.size() * tiles_per_chunk.x * tiles_per_chunk.y);
			for (auto chunk : scene.player_chunks) {
				auto entities = chunk->manager.GetComponentTuple<TransformComponent, CollisionComponent>();
				chunk_entities.insert(chunk_entities.end(), entities.begin(), entities.end());
			}
			CollisionRoutine(player_entities, chunk_entities, &mine);
			scene.manager.Refresh();
		} else {
			scene.manager.UpdateSystem<CollisionSystem>(true);
		}

		//if (timer1.ElapsedMilliseconds() > 1)
		//LOG("timer1: " << timer1.ElapsedMilliseconds());
		//AllocationMetrics::PrintMemoryUsage();

		Timer timer2;
		timer2.Start();
		scene.manager.UpdateSystem<StateMachineSystem>(true);
		scene.ui_manager.UpdateSystem<StateMachineSystem>(true);
		scene.manager.UpdateSystem<DirectionSystem>();
		//scene.manager.UpdateSystem<LifetimeSystem>();
		scene.manager.UpdateSystem<CameraSystem>();
		//AllocationMetrics::PrintMemoryUsage();

		auto& title_ = title_screen.GetComponent<TitleScreenComponent>();
		if (title_.open && scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
			title_.open = false;
		}
		//if (timer2.ElapsedMilliseconds() > 1)
		//LOG("timer2: " << timer2.ElapsedMilliseconds());
		Timer timer3;
		timer3.Start();
		auto camera = scene.GetCamera();
		players = scene.manager.GetComponentTuple<TransformComponent, CollisionComponent, PlayerController>();
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
				auto min_chunk_pos = Floor(transform.position / chunk_size);
				auto max_chunk_pos = Floor((transform.position + collision.collider.size) / chunk_size);
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
						chunk->Generate(0, octave, bias);
						chunk->new_chunk = true;
						scene.chunks.emplace_back(chunk);
						if (player_chunk) {
							scene.player_chunks.emplace_back(chunk);
						}
					}
				}
			}
			//LOG("Chunks: " << chunks.size());
		} else {
			scene.chunks.clear();
		}
		//if (timer3.ElapsedMilliseconds() > 1)
		//LOG("timer3: " << timer3.ElapsedMilliseconds());

		Timer timer4;
		timer4.Start();
		particles.Update();
		//if (timer4.ElapsedMilliseconds() > 1)
		//LOG("timer4: " << timer4.ElapsedMilliseconds());
		//LOG("Octave: " << octave << ", bias: " << bias);
    }
	void Render() {
		auto& scene = Scene::Get();
		Timer timer5;
		timer5.Start();
		scene.manager.UpdateSystem<AnimationSystem>();
		//if (timer5.ElapsedMilliseconds() > 1)
		//LOG("timer5: " << timer5.ElapsedMilliseconds());
		Timer timer6;
		timer6.Start();
		// TODO: Consider a better way of doing this?
		for (auto& c : scene.chunks) {
			c->manager.UpdateSystem<TileRenderSystem>();
		}
		//if (timer6.ElapsedMilliseconds() > 1)
		//LOG("timer6: " << timer6.ElapsedMilliseconds());
		Timer timer7;
		timer7.Start();
		scene.manager.UpdateSystem<RenderSystem>();
		scene.manager.UpdateSystem<HitboxRenderSystem>();
		//if (timer7.ElapsedMilliseconds() > 1)
		//LOG("timer7: " << timer7.ElapsedMilliseconds());

		Timer timer8;
		timer8.Start();
		scene.ui_manager.UpdateSystem<UIRenderer>();
		particles.Render(scene);
		//if (timer8.ElapsedMilliseconds() > 1)
		//LOG("timer8: " << timer8.ElapsedMilliseconds());
	}
private:
	ecs::Entity title_screen;
};

int main(int argc, char* args[]) { // sdl main override

	Engine::Start<Game>("Protegon", 512 * 2, 600, 60);

    return 0;
}