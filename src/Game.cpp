#include "Game.h"

#include <ECS/ECS.h>
#include <ECS/Components.h>
#include <ECS/Systems.h>

#include "InputHandler.h"
#include "TextureManager.h"

std::unique_ptr<Game> Game::_instance = nullptr;
SDL_Window* Game::_window = nullptr;
SDL_Renderer* Game::_renderer = nullptr;
std::vector<std::pair<AABB, SDL_Color>> Game::aabbs;
std::vector<std::tuple<Vec2D, Vec2D, SDL_Color>> Game::lines;
std::vector<std::pair<Vec2D, SDL_Color>> Game::points;
bool Game::_running = false;

SDL_Event event;

ecs::Manager manager;

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

Game& Game::getInstance() {
	if (!_instance) {
		_instance = std::make_unique<Game>();
	}
	return *_instance;
}

SDL_Window* Game::getWindow() {
	return _window;
}
SDL_Renderer* Game::getRenderer() {
	return _renderer;
}

void Game::init() {
	initSDL(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_W, WINDOW_H, WINDOW_FLAGS);
	_running = true;
	TextureManager::getInstance();
	InputHandler::getInstance();
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

	for (size_t i = 0; i < boxes.size(); ++i) {
		for (size_t j = 0; j < boxes[i].size(); ++j) {
			if (boxes[i][j]) {
				Vec2D pos = { static_cast<double>(32 * j), static_cast<double>(32 * i) };
				switch (boxes[i][j]) {
					case 1: {
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

void Game::initSDL(const char* title, int x, int y, int w, int h, Uint32 flags) {
	assert(SDL_Init(SDL_INIT_EVERYTHING) == 0 && "SDL failed to init");
	_window = SDL_CreateWindow(title, x, y, w, h, flags);
	assert(_window != nullptr && "SDL failed to create window");
	_renderer = SDL_CreateRenderer(_window, -1, 0);
	assert(_renderer != nullptr && "SDL failed to create renderer");
}

void Game::instructions() {
	std::cout << "(w, a, s, d) -> move" << std::endl;
	std::cout << "(q) -> zoom in, (e) -> zoom out" << std::endl;
	std::cout << "(r) -> restart game to tutorial" << std::endl;
	std::cout << "(c) -> shoot" << std::endl;
}

void Game::update() {
	InputHandler::update(event);
	static int cycle = 0;
	cycle++;
	manager.Update<InputSystem>();
	manager.Update<PhysicsSystem>();
	manager.Update<CollisionSystem>();
	//manager.Update<StateMachineSystem>();
	manager.Update<DirectionSystem>();
	manager.Update<LifetimeSystem>();
	//AllocationMetrics::printMemoryUsage();
}

static bool equal(SDL_Color o, SDL_Color p) {
	return o.a == p.a && o.b == p.b && o.g == p.g && o.r == p.r;
}

void Game::render() {
	SDL_RenderClear(_renderer);
	TextureManager::setDrawColor(RENDER_COLOR);
	manager.Update<AnimationSystem>();
	manager.Update<RenderSystem>();
	for (auto& point : Game::points) {
		TextureManager::drawPoint(point.first, point.second);
	}
	for (auto& line : Game::lines) {
		TextureManager::drawLine(std::get<0>(line), std::get<1>(line), std::get<2>(line));
	}
	for (auto& box : Game::aabbs) {
		TextureManager::drawRectangle(box.first, box.second);
	}
	SDL_RenderPresent(_renderer);
	aabbs.clear();
	lines.clear();
	points.clear();
}

void Game::loop() {
	const Uint32 fDelay = 1000 / FPS;
	Uint32 fStart;
	Uint32 fTime;
	while (_running) {
		fStart = SDL_GetTicks();
		update();
		render();
		//SDL_Delay(100);
		fTime = SDL_GetTicks() - fStart;
		if (fDelay > fTime) { // cap frame time at an FPS
			SDL_Delay(fDelay - fTime);
		}
	}
}

void Game::clean() {
	SDL_DestroyWindow(_window);
	SDL_DestroyRenderer(_renderer);
	// Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Game::quit() {
	_running = false;
}