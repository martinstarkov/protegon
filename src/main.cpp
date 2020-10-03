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
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 3},
		{3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 1, 0, 1, 3},
		{3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3}
		};

		for (std::size_t i = 0; i < boxes.size(); ++i) {
			for (std::size_t j = 0; j < boxes[i].size(); ++j) {
				if (boxes[i][j]) {
					Vec2D pos = { 32.0 * j, 32.0 * i };
					switch (boxes[i][j]) {
						case 1:
						{
							ecs::Entity b = CreateBox(pos, manager);
							RigidBody rb = RigidBody(UNIVERSAL_DRAG, GRAVITY);
							b.AddComponent<RigidBodyComponent>(rb);
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

	ecs::Entity CreateBox(Vec2D position, ecs::Manager& manager) {
		auto entity = manager.CreateEntity();
		entity.AddComponent<RenderComponent>();
		entity.AddComponent<CollisionComponent>(position, Vec2D{ 32, 32 });
		entity.AddComponent<SpriteComponent>("./resources/textures/box.png", Vec2D{ 32, 32 });
		entity.AddComponent<TransformComponent>(position);
		return entity;
	}

	ecs::Entity CreatePlayer(Vec2D position, ecs::Manager& manager) {
		auto entity = manager.CreateEntity();
		Vec2D playerAcceleration = Vec2D(8, 20);
		entity.AddComponent<TransformComponent>(position);
		entity.AddComponent<InputComponent>();
		entity.AddComponent<PlayerController>(playerAcceleration);
		entity.AddComponent<RigidBodyComponent>(RigidBody(UNIVERSAL_DRAG, GRAVITY, ELASTIC, INFINITE, abs(playerAcceleration) + abs(GRAVITY)));
		entity.AddComponent<CollisionComponent>(position, Vec2D{ 30, 51 });
		entity.AddComponent<SpriteComponent>("./resources/textures/player_test2.png", Vec2D(30, 51));
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