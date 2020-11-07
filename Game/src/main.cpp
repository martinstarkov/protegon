#include <engine/Include.h>

struct TitleScreenComponent {
	TitleScreenComponent(bool open = false) : open{ open } {}
	bool open;
};

struct PauseScreenComponent {
	PauseScreenComponent() = default;
	bool toggleable = true;
	bool open = false;
	int release_time = 0;
};

ecs::Entity CreateBox(V2_double position, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CollisionComponent>(position, V2_double{ 32, 32 });
	entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_int{ 32, 32 });
	entity.AddComponent<TransformComponent>(position);
	return entity;
}

ecs::Entity CreatePlayer(V2_double position, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	V2_double player_acceleration = { 2, 4 };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<InputComponent>();
	entity.AddComponent<PlayerController>(player_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, V2_double{ 0, 0.8 }, ELASTIC, INFINITE_MASS, abs(player_acceleration) + abs(GRAVITY) });
	entity.AddComponent<CollisionComponent>(position, V2_double{ 32, 64 });
	entity.AddComponent<SpriteComponent>("./resources/textures/player_test2.png", V2_int{ 30, 51 });
	entity.AddComponent<SpriteSheetComponent>();
	//entity.AddComponent<StateMachineComponent>(entity, RawStateMachineMap{ { "walkStateMachine", new WalkStateMachine("idle") }, { "jumpStateMachine", new JumpStateMachine("grounded") }});
	entity.AddComponent<DirectionComponent>();
	entity.AddComponent<AnimationComponent>();
	entity.AddComponent<RenderComponent>();
	return entity;
}

struct RandomizeColorEvent {
	static void Invoke(ecs::Entity& invoker) {
		auto& influence_manager = *invoker.GetComponent<UIComponent>()->GetInfluenceManager();
		auto color_entities = influence_manager.GetComponentTuple<RenderComponent>();
		for (auto [entity2, render_component2] : color_entities) {
			render_component2.original_color = engine::Color::RandomSolid();
		}
		// Hi
		//influence_manager.RemoveSystem<PhysicsSystem>();
	}
};

struct GameStartEvent {
	static void Invoke(ecs::Entity& invoker) {
		if (invoker.IsAlive()) {
			auto& influence_manager = *invoker.GetComponent<UIComponent>()->GetInfluenceManager();
			influence_manager.Clear();
			invoker.GetManager()->DestroyEntitiesWith<TitleScreenComponent>();
			std::vector<std::vector<int>> boxes = {
			{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 3},
			{3, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 3},
			{3, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 3},
			{3, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 3},
			{3, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 1, 0, 0, 0, 0, 1, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 1, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
			{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
			{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}
			};

			for (std::size_t i = 0; i < boxes.size(); ++i) {
				for (std::size_t j = 0; j < boxes[i].size(); ++j) {
					if (boxes[i][j]) {
						auto pos = V2_double{ 32.0 * j, 32.0 * i };
						switch (boxes[i][j]) {
							case 1:
							{
								auto box = CreateBox(pos, influence_manager);
								auto rb = RigidBody{ UNIVERSAL_DRAG, GRAVITY };
								box.AddComponent<RigidBodyComponent>(rb);
								break;
							}
							case 2:
							{
								auto player = CreatePlayer(pos, influence_manager);
								break;
							}
							case 3:
							{
								auto box = CreateBox(pos, influence_manager);
								break;
							}
							default:
								break;
						}
					}
				}
			}

			auto button = new engine::UIButton("Randomize Color", 15, "resources/fonts/oswald_regular.ttf", engine::BLUE, engine::SILVER, engine::GOLD, engine::RED, &influence_manager);

			engine::UI::AddInteractable<RandomizeColorEvent>(invoker.GetManager(), { 40, 40 }, { 120, 40 }, button);
		}
	}
};

struct PauseScreenEvent {
	static void Invoke(ecs::Entity& invoker, ecs::Manager& manager, ecs::Manager& ui_manager) {
		auto& pause = invoker.GetComponent<PauseScreenComponent>();
		if (pause.open) {
			pause.open = false;
			ui_manager.DestroyEntitiesWith<PauseScreenComponent>();
		} else {
			pause.open = true;
			auto pause_text = new engine::UITextBox("Paused", 30, "resources/fonts/oswald_regular.ttf", engine::WHITE, engine::BLACK);
			V2_int sb1 = { 200, 100 };
			V2_int pb1 = engine::Engine::ScreenSize() / 2 - sb1 / 2;
			auto b1 = engine::UI::AddStatic(&ui_manager, pb1, sb1, pause_text);
			b1.AddComponent<PauseScreenComponent>();
		}
	}
};

struct TitleScreenEvent {
	static void Invoke(ecs::Manager& manager, ecs::Manager& ui_manager) {
		manager.Clear();
		ui_manager.Clear();

		auto play_button = new engine::UIButton("Play", 30, "resources/fonts/oswald_regular.ttf", engine::WHITE, engine::BLACK, engine::GREY, engine::SILVER, &manager);
		V2_int sb1 = { 200, 100 };
		V2_int pb1 = engine::Engine::ScreenSize() / 2 - sb1 / 2;
		auto b1 = engine::UI::AddInteractable<GameStartEvent>(&ui_manager, pb1, sb1, play_button);
		b1.AddComponent<TitleScreenComponent>();

		auto instructions_button = new engine::UIButton("Instructions", 20, "resources/fonts/oswald_regular.ttf", engine::WHITE, engine::BLACK, engine::GREY, engine::SILVER, &manager);
		V2_int sb2 = { 150, 80 };
		V2_int pb2 = engine::Engine::ScreenSize() / 2 - sb2 / 2;
		pb2.y += sb1.y + 30;
		auto b2 = engine::UI::AddInteractable<GameStartEvent>(&ui_manager, pb2, sb2, instructions_button);
		b2.AddComponent<TitleScreenComponent>();
	}
};

class MyGame : public engine::Engine {
public:
	void Init() {
		manager.AddSystem<RenderSystem>();
		manager.AddSystem<PhysicsSystem>();
		manager.AddSystem<LifetimeSystem>();
		manager.AddSystem<AnimationSystem>();
		manager.AddSystem<CollisionSystem>();
		manager.AddSystem<InputSystem>();
		//manager.AddSystem<StateMachineSystem>();
		manager.AddSystem<DirectionSystem>();
		ui_manager.AddSystem<RenderSystem>();
		ui_manager.AddSystem<UIListener>();
		ui_manager.AddSystem<UIRenderer>();

		title_screen = event_manager.CreateEntity();
		engine::EventHandler::Register<TitleScreenEvent>(title_screen);
		auto& title = title_screen.AddComponent<TitleScreenComponent>();
		pause_screen = event_manager.CreateEntity();
		engine::EventHandler::Register<PauseScreenEvent>(pause_screen);
		auto& pause = pause_screen.AddComponent<PauseScreenComponent>();
		engine::EventHandler::Invoke(title_screen, manager, ui_manager);
		title.open = true;
		pause.open = false;
	}

    void Update() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		manager.Update<InputSystem>();
		if (!pause.open) {
			ui_manager.Update<UIListener>();
			if (manager.HasSystem<PhysicsSystem>()) {
				manager.Update<PhysicsSystem>();
			}
			manager.Update<CollisionSystem>();
			//manager.Update<StateMachineSystem>();
			manager.Update<DirectionSystem>();
			manager.Update<LifetimeSystem>();
		}
		//AllocationMetrics::printMemoryUsage();
		auto& title = title_screen.GetComponent<TitleScreenComponent>();
		if (engine::InputHandler::KeyPressed(Key::R)) {
			engine::EventHandler::Invoke(title_screen, manager, ui_manager);
			title.open = true;
			pause.open = false;
			//MemTrack::TrackListMemoryUsage();
		} else if (title.open) {
			if (ui_manager.GetEntitiesWith<TitleScreenComponent>().size() == 0) {
				title.open = false;
			}
		}
		if (engine::InputHandler::KeyPressed(Key::ESCAPE) && pause.toggleable && !title.open) {
			pause.toggleable = false;
			engine::EventHandler::Invoke(pause_screen, pause_screen, manager, ui_manager);
			//MemTrack::TrackListMemoryUsage();
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
			manager.Update<AnimationSystem>();
		}
		manager.Update<RenderSystem>();
		ui_manager.Update<UIRenderer>();
	}
private:
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	engine::Engine::Start<MyGame>("Protegon");

    return 0;
}