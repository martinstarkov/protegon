#include <engine/core/Engine.h>

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
							CreatePlayer(pos, manager);
							break;
						case 3:
							CreateBox(pos, manager);
							break;
						default:
							break;
					}
				}
			}
		}

	}
    void Update() {
		manager.Update<InputSystem>();
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
		V2_double player_acceleration = { 8, 20 };
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<InputComponent>();
		entity.AddComponent<PlayerController>(player_acceleration);
		entity.AddComponent<RigidBodyComponent>(RigidBody{ UNIVERSAL_DRAG, GRAVITY, ELASTIC, INFINITE_MASS, abs(player_acceleration) + abs(GRAVITY) });
		entity.AddComponent<CollisionComponent>(position, V2_double{ 30, 51 });
		entity.AddComponent<SpriteComponent>("./resources/textures/player_test2.png", V2_double{ 30, 51 });
		entity.AddComponent<SpriteSheetComponent>();
		//entity.AddComponent<StateMachineComponent>(entity, RawStateMachineMap{ { "walkStateMachine", new WalkStateMachine("idle") }, { "jumpStateMachine", new JumpStateMachine("grounded") }});
		entity.AddComponent<DirectionComponent>();
		entity.AddComponent<AnimationComponent>();
		entity.AddComponent<RenderComponent>();
		return entity;
	}
};

int main(int argc, char* args[]) { // sdl main override

	engine::Engine::Start<MyGame>();

    return 0;
}