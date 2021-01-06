#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

#include <cmath>
#include <set>

class MyGame : public engine::Engine {
public:
	std::vector<engine::Image> images;
	V2_int tile_size = { 32, 32 };
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
		pause_screen = scene.event_manager.CreateEntity();
		engine::EventHandler::Register<PauseScreenEvent>(pause_screen);
		auto& pause = pause_screen.AddComponent<PauseScreenComponent>();
		engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
		title.open = true;
		pause.open = false;
		LOG("Initialized all game systems successfully");
	}
	struct cmp {
		bool operator() (AABB a, AABB b) const {
			return a == b;
		}
	};
	// TODO: Move chunks vector elsewhere.
	std::vector<engine::Chunk*> chunks;
	bool Contains(std::vector<engine::Chunk*> vector, AABB value) {
		for (auto& v : vector) {
			if (v->GetInfo() == value) { return true; }
		}
		return false;
	}
	bool Contains(std::vector<AABB> vector, AABB value) {
		for (auto& v : vector) {
			if (v == value) { return true; }
		}
		return false;
	}
	int octave = 8;
	double bias = 8.0;
    void Update() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		scene.manager.Update<InputSystem>();
		if (!pause.open) {
			scene.ui_manager.Update<UIButtonListener>();
			if (scene.manager.HasSystem<PhysicsSystem>()) {
				scene.manager.Update<PhysicsSystem>();
				scene.manager.Update<TargetSystem>();
			}
			scene.manager.Update<CollisionSystem>();
			scene.manager.Update<StateMachineSystem>();
			scene.manager.Update<DirectionSystem>();
			scene.manager.Update<LifetimeSystem>();
			scene.manager.Update<CameraSystem>();
		}
		//AllocationMetrics::PrintMemoryUsage();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyPressed(Key::R)) {
			engine::EventHandler::Invoke(title_screen, scene.manager, scene.ui_manager);
			title.open = true;
			pause.open = false;
		} else if (title.open) {
			if (scene.ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
				title.open = false;
			}
		}

		if (engine::InputHandler::KeyDown(Key::X))
			octave++;

		if (engine::InputHandler::KeyDown(Key::C))
			octave--;

		if (engine::InputHandler::KeyDown(Key::F))
			bias += 0.2;

		if (engine::InputHandler::KeyDown(Key::G))
			bias -= 0.2;

		if (bias < 0.2)
			bias = 0.2;

		if (octave < 1)
			octave = 1;

		LOG("octave: " << octave);

		// TODO: Massive cleanup...

		auto camera = scene.GetCamera();
		if (camera && !title.open) {
			V2_double tiles_per_chunk = V2_double{ 16, 16 };
			V2_double tile_size = { 32, 32 };
			V2_double chunk_size = tiles_per_chunk * tile_size;
			V2_double lowest = (camera->offset / chunk_size).Floor();
			V2_double highest = ((camera->offset + static_cast<V2_double>(engine::Engine::ScreenSize()) / camera->scale) / chunk_size).Floor();
			// Optional: Expand loaded chunk region.
			/*lowest += -1;
			highest += 1;*/
			std::vector<AABB> potential_chunks;
			assert(lowest.x <= highest.x && "Left grid edge cannot be above right grid edge");
			assert(lowest.y <= highest.y && "Top grid edge cannot be below top grid edge");
			for (auto i = lowest.x; i != highest.x + 1; ++i) {
				for (auto j = lowest.y; j != highest.y + 1; ++j) {
					V2_double grid_loc = { i, j };
					auto pos = chunk_size * grid_loc;
					auto potential_chunk = AABB{ pos, tiles_per_chunk };
					potential_chunks.push_back(potential_chunk);
					//DebugDisplay::rectangles().emplace_back(AABB{ pos, chunk_size }, engine::ORANGE);
				}
			}
			/*AABB broadphase_chunk = { lowest * chunk_size, (highest - lowest + V2_double{ 1, 1}).Absolute() * chunk_size };
			DebugDisplay::rectangles().emplace_back(broadphase_chunk, engine::DARK_GREEN);*/

			std::vector<AABB> new_chunks;
			std::vector<AABB> removed_chunks;

			// TODO: Fix this mess.

			for (auto& p : potential_chunks) {

				bool new_chunk = true;

				for (auto& c : chunks) {

					if (p == c->GetInfo()) {
						new_chunk = false;
					}

				}

				if (new_chunk) {
					new_chunks.push_back(p);
				}

			}

			for (auto& c : chunks) {

				bool removed_chunk = true;

				for (auto& p : potential_chunks) {

					if (p == c->GetInfo()) {
						removed_chunk = false;
					}

				}

				if (removed_chunk) {
					removed_chunks.push_back(c->GetInfo());
				}

			}

			auto it = chunks.begin();
			while (it != chunks.end()) {
				if (Contains(removed_chunks, (*it)->GetInfo())) {
					delete (*it);
					it = chunks.erase(it);
				} else {
					++it;
				}
			}

			for (auto n : new_chunks) {

				if (!Contains(chunks, n)) {

					auto chunk = new BoxChunk();
					chunk->Init(n, tile_size, &scene);
					engine::Timer timer;
					timer.Start();
					chunk->Generate(1, octave, bias);
					//LOG("Generate: " << timer.ElapsedMilliseconds());

					chunks.push_back(chunk);
				}

			}

			auto players = scene.manager.GetComponentTuple<PlayerController, TransformComponent>();
			for (auto c : chunks) {

				for (auto [entity, player, transform] : players) {
					if (engine::collision::PointvsAABB(transform.position, c->GetInfo())) {
						if (engine::InputHandler::KeyPressed(Key::P)) {
							c->perlin.PrintNoise(c->perlin.fPerlinNoise2D);
						}
					}
				}

			}
			//LOG("Chunks: " << chunks.size());
		} else {
			for (auto c : chunks) {
				delete c;
				c = nullptr;
			}
			chunks.clear();
        }
		//LOG("Octave: " << octave << ", bias: " << bias);
		if (engine::InputHandler::KeyPressed(Key::ESCAPE) && pause.toggleable && !title.open) {
			pause.toggleable = false;
			engine::EventHandler::Invoke(pause_screen, pause_screen, scene.manager, scene.ui_manager);
		} else if (engine::InputHandler::KeyReleased(Key::ESCAPE)) {
			if (!pause.toggleable) {
				pause.release_time += 1;
			}
			if (pause.release_time >= 5) {
				pause.toggleable = true;
				pause.release_time = 0;
			}
		}
    }
	void Render() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		if (!pause.open) {
			scene.manager.Update<AnimationSystem>();
		}
		// TODO: Consider a more automatic way of doing this?
		for (auto& c : chunks) {
			c->manager.Update<RenderSystem>();
			c->manager.Update<HitboxRenderSystem>();
		}
		scene.manager.Update<RenderSystem>();
		scene.manager.Update<HitboxRenderSystem>();
		scene.ui_manager.Update<UIButtonRenderer>();
		scene.ui_manager.Update<UITextRenderer>();
	}
private:
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Protegon");
	engine::Engine::Start<MyGame>("Protegon", 1000, 600);

    return 0;
}