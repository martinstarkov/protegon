#include "Game.h"

#include <cassert>

namespace engine {

BaseGame* Engine::game{ nullptr };
SDL_Window* Engine::window{ nullptr };
SDL_Renderer* Engine::renderer{ nullptr };
bool Engine::running{ false };
// Defined in Init()
int Engine::window_width{ -1 };
int Engine::window_height{ -1 };
int Engine::window_x{ -1 };
int Engine::window_y{ -1 };
int Engine::frame_rate{ -1 };
const char* Engine::window_title{ "" };

SDL_Window& Engine::GetWindow() { assert(window != nullptr); return *window; }
SDL_Renderer& Engine::GetRenderer() { assert(renderer != nullptr); return *renderer; }
int Engine::ScreenWidth() { return window_width; }
int Engine::ScreenHeight() { return window_height; }
int Engine::FPS() { return frame_rate; }

void Engine::Init(const char* title, int width, int height, int fps, int x, int y, std::uint32_t window_flags, std::uint32_t renderer_flags) {
	window_title = title;
	window_width = width;
	window_height = height;
	frame_rate = fps;
	window_x = x;
	window_y = x;
	InitSDL(window_flags, renderer_flags);


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

	for (size_t i = 0; i < boxes.size(); ++i) {
		for (size_t j = 0; j < boxes[i].size(); ++j) {
			if (boxes[i][j]) {
				Vec2D pos = { static_cast<double>(32 * j), static_cast<double>(32 * i) };
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

	running = true;
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

void Engine::InitSDL(std::uint32_t window_flags, std::uint32_t renderer_flags) {
	if (SDL_Init(SDL_INIT_EVERYTHING) == 0) {
		window = SDL_CreateWindow(window_title, window_x, window_y, window_width, window_height, window_flags);
		if (window) {
			renderer = SDL_CreateRenderer(window, -1, renderer_flags);
			if (renderer) {
				// SDL fully initialized
			} else {
				assert(!"SDL failed to create renderer");
			}
		} else {
			assert(!"SDL failed to create window");
		}
	} else {
		assert(!"SDL failed to initialize");
	}
}

void Engine::Clean() {
	TextureManager::Clean();
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	// Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Engine::Quit() { running = false; }

} // namespace engine