#include <engine/Engine.h>

class MyGame : public engine::Game {
	void Init() {
		manager.AddSystem<RenderSystem>();
		manager.AddSystem<PhysicsSystem>();
		manager.AddSystem<LifetimeSystem>();
		manager.AddSystem<AnimationSystem>();
		manager.AddSystem<CollisionSystem>();
		manager.AddSystem<InputSystem>();
		//manager.AddSystem<StateMachineSystem>();
		manager.AddSystem<DirectionSystem>();
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
};

int main(int argc, char* args[]) { // sdl main override

    engine::Game::Play();

    return 0;
}