#include "Game.h"
#include "ECS/Manager.h"
#include "ECS/Entity.h"
#include "InputHandler.h"
#include "TextureManager.h"

std::unique_ptr<Game> Game::_instance = nullptr;
SDL_Window* Game::_window = nullptr;
SDL_Renderer* Game::_renderer = nullptr;
std::vector<AABB> Game::aabbs;
std::vector<std::pair<Vec2D, Vec2D>> Game::lines;
std::vector<Vec2D> Game::points;
bool Game::_running = false;

SDL_Event event;

Manager manager;

Entity box1;
Entity player;

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
	manager.init();

	box1 = Entity(manager.createBox(Vec2D(32 * 4, 32 * 4)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 4, 32 * 5)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 4, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 5, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 6, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 7, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 8, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 9, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 10, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 11, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 12, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 13, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 14, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 15, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 16, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 17, 32 * 6)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 17, 32 * 5)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 17, 32 * 4)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 17, 32 * 3)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 16, 32 * 3)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 15, 32 * 3)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 14, 32 * 3)), &manager);
	box1 = Entity(manager.createBox(Vec2D(32 * 13, 32 * 3)), &manager);
	player = Entity(manager.createPlayer(Vec2D(30 * 10, 30)), &manager);

	manager.refresh();
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
	manager.update();
	//AllocationMetrics::printMemoryUsage();
}

void Game::render() {
	SDL_Delay(1000);
	SDL_RenderClear(_renderer);
	TextureManager::setDrawColor(DEFAULT_RENDER_COLOR);
	manager.render();
	for (auto& box : Game::aabbs) {
		TextureManager::draw(Util::RectFromAABB(box), { 0, 0, 255, 255 });
	}
	TextureManager::setDrawColor({ 255, 0, 255, 255 });
	for (auto& line : Game::lines) {
		SDL_RenderDrawLine(_renderer, line.first.x, line.first.y, line.second.x, line.second.y);
	}
	TextureManager::setDrawColor({ 255, 0, 0, 255 });
	for (auto& point : Game::points) {
		SDL_RenderDrawPoint(_renderer, point.x, point.y);
	}
	TextureManager::setDrawColor(DEFAULT_RENDER_COLOR);
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