#include <engine/core/Engine.h>

class MyGame : public engine::Engine {
public:
	void Init() {
		ecs.AddSystem<RenderSystem>();
		ecs.AddSystem<PhysicsSystem>();
		ecs.AddSystem<LifetimeSystem>();
		ecs.AddSystem<AnimationSystem>();
		ecs.AddSystem<CollisionSystem>();
		ecs.AddSystem<InputSystem>();
		//ecs.AddSystem<StateMachineSystem>();
		ecs.AddSystem<DirectionSystem>();
		ui_manager.AddSystem<RenderSystem>();
		ui_manager.AddSystem<UIListener>();
		ui_manager.AddSystem<UIRenderer>();

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

		/*
		std::vector<std::vector<int>> boxes = {
		{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3},
		{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}
		};
		*/

		for (std::size_t i = 0; i < boxes.size(); ++i) {
			for (std::size_t j = 0; j < boxes[i].size(); ++j) {
				if (boxes[i][j]) {
					auto pos = V2_double{ 32.0 * j, 32.0 * i };
					switch (boxes[i][j]) {
						case 1:
						{
							auto box = CreateBox(pos, ecs);
							auto rb = RigidBody{ UNIVERSAL_DRAG, GRAVITY };
							box.AddComponent<RigidBodyComponent>(rb);
							break;
						}
						case 2:
						{
							auto player = CreatePlayer(pos, ecs);
							break;
						}
						case 3:
						{
							auto box = CreateBox(pos, ecs);
							break;
						}
						default:
							break;
					}
				}
			}
		}

		engine::UI::AddButton(ui_manager, { 40, 40 }, { 40, 40 }, engine::UIElement("Button1", engine::BLUE));

	}

    void Update() {
		ecs.Update<InputSystem>();
		ui_manager.Update<UIListener>();
		ecs.Update<PhysicsSystem>();
		ecs.Update<CollisionSystem>();
		//ecs.Update<StateMachineSystem>();
		ecs.Update<DirectionSystem>();
		ecs.Update<LifetimeSystem>();
		//AllocationMetrics::printMemoryUsage();
    }

	void Render() {
		ecs.Update<AnimationSystem>();
		ecs.Update<RenderSystem>();
		ui_manager.Update<UIRenderer>();
	}

	ecs::Entity CreateBox(V2_double position, ecs::Manager& manager) {
		auto entity = manager.CreateEntity();
		entity.AddComponent<RenderComponent>();
		entity.AddComponent<CollisionComponent>(position, V2_double{ 32, 32 });
		entity.AddComponent<SpriteComponent>("./resources/textures/box.png", V2_double{ 32, 32 });
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
		entity.AddComponent<SpriteComponent>("./resources/textures/player_test2.png", V2_double{ 32, 64 });
		entity.AddComponent<SpriteSheetComponent>();
		//entity.AddComponent<StateMachineComponent>(entity, RawStateMachineMap{ { "walkStateMachine", new WalkStateMachine("idle") }, { "jumpStateMachine", new JumpStateMachine("grounded") }});
		entity.AddComponent<DirectionComponent>();
		entity.AddComponent<AnimationComponent>();
		entity.AddComponent<RenderComponent>();
		return entity;
	}
};

int main(int argc, char* args[]) { // sdl main override

	engine::Engine::Start<MyGame>("Protegon");

    return 0;
}