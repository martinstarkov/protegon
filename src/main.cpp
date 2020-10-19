#include <engine/core/Engine.h>
#include <engine/core/Includes.h>

#include <memory>

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
	static void Invoke(ecs::Entity& invoker, ecs::Manager& manager, ecs::Manager& ui_manager) {
		auto color_entities = manager.GetComponentTuple<RenderComponent>();
		for (auto [entity2, render_component2] : color_entities) {
			render_component2.original_color = engine::Color::RandomSolid();
		}
	}
};

struct GameStartEvent {
	static void Invoke(ecs::Entity& invoker, ecs::Manager& manager, ecs::Manager& ui_manager) {

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
							auto box = CreateBox(pos, manager);
							auto rb = RigidBody{ UNIVERSAL_DRAG, GRAVITY };
							box.AddComponent<RigidBodyComponent>(rb);
							break;
						}
						case 2:
						{
							auto player = CreatePlayer(pos, manager);
							break;
						}
						case 3:
						{
							auto box = CreateBox(pos, manager);
							break;
						}
						default:
							break;
					}
				}
			}
		}
		invoker.Destroy();

		engine::UI::AddButton<RandomizeColorEvent>(ui_manager, { 40, 40 }, { 120, 40 }, engine::UIElement("Randomize Color", 15, "resources/fonts/oswald_regular.ttf", engine::BLUE, engine::SILVER, engine::GOLD, engine::RED, &manager));
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

		engine::UI::AddButton<GameStartEvent>(ui_manager, { ScreenWidth() / 2 - 40, ScreenHeight() / 2 - 30 }, { 80, 60 }, engine::UIElement("Play", 30, "resources/fonts/oswald_regular.ttf", engine::RED, engine::SILVER, engine::GOLD, engine::RED, &manager));

	}

    void Update() {
		manager.Update<InputSystem>();
		ui_manager.Update<UIListener>();
		manager.Update<PhysicsSystem>();
		manager.Update<CollisionSystem>();
		//manager.Update<StateMachineSystem>();
		manager.Update<DirectionSystem>();
		manager.Update<LifetimeSystem>();
		//AllocationMetrics::printMemoryUsage();
    }

	void Render() {
		manager.Update<AnimationSystem>();
		manager.Update<RenderSystem>();
		ui_manager.Update<UIRenderer>();
	}
};

int main(int argc, char* args[]) { // sdl main override

	engine::Engine::Start<MyGame>("Protegon");

    return 0;
}