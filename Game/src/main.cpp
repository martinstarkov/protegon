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
	//entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_int{ 32, 32 });
	entity.AddComponent<TransformComponent>(position);
	return entity;
}

ecs::Entity CreatePlayer(V2_double position, V2_double size, ecs::Manager& manager) {
	auto entity = manager.CreateEntity();
	V2_double player_acceleration = { 2, 4 };
	entity.AddComponent<TransformComponent>(position);
	entity.AddComponent<InputComponent>();
	entity.AddComponent<PlayerController>(player_acceleration);
	entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, V2_double{ 0, 0.8 }, ELASTIC, INFINITE_MASS, abs(player_acceleration) + abs(GRAVITY) });
	entity.AddComponent<CollisionComponent>(position, size);
	//entity.AddComponent<SpriteComponent>("./resources/textures/player_test2.png", V2_int{ 30, 51 });
	entity.AddComponent<SpriteSheetComponent>();
	//entity.AddComponent<StateMachineComponent>(entity, RawStateMachineMap{ { "walkStateMachine", new WalkStateMachine("idle") }, { "jumpStateMachine", new JumpStateMachine("grounded") }});
	entity.AddComponent<DirectionComponent>();
	entity.AddComponent<AnimationComponent>();
	entity.AddComponent<RenderComponent>();
	entity.AddComponent<CameraComponent>(engine::Camera{}, true);
	return entity;
}

class MitosisSystem : public ecs::System<PlayerController, TransformComponent, CollisionComponent, RigidBodyComponent> {
	virtual void Update() override final {
		for (auto [entity, player, transform, collider, rigid_body] : entities) {
			if (engine::InputHandler::KeyDown(Key::M)) {
				CreatePlayer({ transform.position.x, transform.position.y - 1 }, { collider.collider.size.x, collider.collider.size.y / 2 }, GetManager());
				CreatePlayer({ transform.position.x, transform.position.y + collider.collider.size.y - 1 }, { collider.collider.size.x, collider.collider.size.y / 2 }, GetManager());
				entity.Destroy();
			}
		}
	}
};

struct RandomizeColorEvent {
	static void Invoke(ecs::Entity& invoker) {
		if (invoker.HasComponent<InfluenceComponent>()) {
			auto& manager = invoker.GetComponent<InfluenceComponent>().manager;
			/*auto color_entities = manager.GetComponentTuple<RenderComponent>();
			for (auto [entity2, render_component2] : color_entities) {
				render_component2.original_color = engine::Color::RandomSolid();
			}*/
			auto camera_entities = manager.GetComponentTuple<CameraComponent>();
			for (auto [entity, camera] : camera_entities) {
				camera.primary = false;
			}
			auto entities = manager.GetComponentTuple<RenderComponent>();
			auto count = entities.size();
			auto random_index = engine::math::GetRandomValue<std::size_t>(0, count - 1);
			for (std::size_t i = 0; i < count; ++i) {
				if (i == random_index) {
					auto [entity, render] = entities[i];
					entity.AddComponent<CameraComponent>(engine::Camera{}, true);
				}
			}
		}
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
								auto player = CreatePlayer(pos, V2_double{ 32, 64 }, influence_manager);
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
			auto button = engine::UI::AddButton<RandomizeColorEvent>(*invoker.GetManager(), influence_manager, { 40, 40 }, { 120, 40 }, engine::SILVER);
			button.AddComponent<HoverColorComponent>(engine::GOLD);
			button.AddComponent<ActiveColorComponent>(engine::RED);
			button.AddComponent<TextComponent>("Randomize Color", engine::BLUE, 15, "resources/fonts/oswald_regular.ttf");
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
		LOG("Initializing game systems...");
		scene.manager.AddSystem<RenderSystem>(&scene);
		scene.manager.AddSystem<PhysicsSystem>();
		scene.manager.AddSystem<LifetimeSystem>();
		scene.manager.AddSystem<AnimationSystem>();
		scene.manager.AddSystem<CollisionSystem>();
		scene.manager.AddSystem<InputSystem>();
		//scene.manager.AddSystem<StateMachineSystem>();
		scene.manager.AddSystem<DirectionSystem>();
		scene.manager.AddSystem<MitosisSystem>();
		scene.manager.AddSystem<CameraSystem>(&scene);
		scene.ui_manager.AddSystem<RenderSystem>();
		scene.ui_manager.AddSystem<UIListener>();
		scene.ui_manager.AddSystem<UIRenderer>();
		scene.ui_manager.AddSystem<UIButtonListener>();
		scene.ui_manager.AddSystem<UIButtonRenderer>();

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

    void Update() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		scene.manager.Update<InputSystem>();
		if (!pause.open) {
			scene.ui_manager.Update<UIListener>();
			scene.ui_manager.Update<UIButtonListener>();
			if (scene.manager.HasSystem<PhysicsSystem>()) {
				scene.manager.Update<PhysicsSystem>();
			}
			scene.manager.Update<CollisionSystem>();
			//scene.manager.Update<StateMachineSystem>();
			scene.manager.Update<DirectionSystem>();
			scene.manager.Update<LifetimeSystem>();
			scene.manager.Update<MitosisSystem>();
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
		auto players = scene.manager.GetComponentTuple<PlayerController, TransformComponent>();
		for (auto [entity, player, transform] : players) {
			if (engine::InputHandler::KeyPressed(Key::RIGHT) && engine::InputHandler::KeyReleased(Key::LEFT)) {
				transform.rotation += 1;
			} else if (engine::InputHandler::KeyPressed(Key::LEFT) && engine::InputHandler::KeyReleased(Key::RIGHT)) {
				transform.rotation -= 1;
			}
		}
    }

	void Render() {
		auto& pause = pause_screen.GetComponent<PauseScreenComponent>();
		if (!pause.open) {
			scene.manager.Update<AnimationSystem>();
		}
		scene.manager.Update<RenderSystem>();
		scene.ui_manager.Update<UIRenderer>();
		scene.ui_manager.Update<UIButtonRenderer>();
	}
private:
	ecs::Entity title_screen;
	ecs::Entity pause_screen;
};

int main(int argc, char* args[]) { // sdl main override

	LOG("Starting Protegon");
	engine::Engine::Start<MyGame>("Protegon");

    return 0;
}