#include <engine/Include.h>

#include "components/Components.h"
#include "event/Events.h"
#include "factory/Factories.h"
#include "systems/Systems.h"

#include <cmath>

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
		screen.SetSize(engine::Engine::ScreenSize() / tile_size);
		LOG("Initialized all game systems successfully");
	}

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
		//AllocationMetrics::printMemoryUsage();
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


		auto sectors = screen.GetSize();
		V2_int sector = { 0, 0 };
		for (sector.x = 0; sector.x < sectors.x; ++sector.x) {
			for (sector.y = 0; sector.y < sectors.y; ++sector.y) {
				auto& entity = screen.GetEntity(sector);
				if (entity.IsAlive()) {
					entity.IsValid();
				}
				auto b = 1;
				b += 1;
			}
		}
		auto camera = scene.GetCamera();
		if (camera) {
			LOG(camera->offset);
		}
		//auto camera = scene.GetCamera();
		//if (camera) {
		//	V2_int sector = { 0, 0 };
		//	engine::RNG rng;
		//	auto sectors = screen.GetSize();
		//	LOG("Generating world");
		//	for (sector.x = 0; sector.x < sectors.x; ++sector.x) {
		//		for (sector.y = 0; sector.y < sectors.y; ++sector.y) {
		//			V2_int world_sector = -static_cast<V2_int>(camera->offset) + sector;
		//			auto seed = (world_sector.x & 0xFFFF) << 16 | (world_sector.y & 0xFFFF);
		//			//auto seed = world_sector.x + world_sector.y * sectors.x;
		//			rng.SetSeed(seed);
		//			bool draw_box = (rng.RandomInt(0, 40) == 1);
		//			if (draw_box) {
		//				auto diameter = rng.RandomDouble(100.0, 400.0);
		//				auto color = colors[rng.RandomInt(0, 8)];
		//				auto coordinate = sector * tile_size;// - static_cast<V2_int>(camera->offset);
		//				//auto box = CreateBox(coordinate, tile_size, scene.manager);
		//				engine::TextureManager::DrawCircle(coordinate, (int)diameter / (tile_size.x / 2), color);
		//			}
		//		}
		//	}
		//}
		scene.manager.Update<RenderSystem>();
		scene.manager.Update<HitboxRenderSystem>();
		scene.ui_manager.Update<UIButtonRenderer>();
		scene.ui_manager.Update<UITextRenderer>();
	}
private:
	engine::Level screen{ scene.manager };
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Protegon");
	engine::Engine::Start<MyGame>("Protegon", 1000, 600);

    return 0;
}