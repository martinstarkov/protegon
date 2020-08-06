#include "Game.h"
#include "ECS/Manager.h"
#include "ECS/Entity.h"
#include "ECS/Components.h"
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

Manager manager;

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

	for (int i = 0; i < boxes.size(); ++i) {
		for (int j = 0; j < boxes[i].size(); ++j) {
			if (boxes[i][j]) {
				Vec2D pos = { 32 * j, 32 * i };
				switch (boxes[i][j]) {
					case 1: {
						Entity b = Entity(manager.createBox(pos), &manager);
						RigidBody rb = RigidBody(Vec2D(0.01), Vec2D(0.0, 0.1));
						b.addComponent(RigidBodyComponent(rb));
						break;
					}
					case 2:
						manager.createPlayer(pos);
						break;
					case 3:
						manager.createBox(pos);
						break;
					default:
						break;
				}
			}
		}
	}
	//manager.createBox(Vec2D(144, 100));
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

static bool equal(SDL_Color o, SDL_Color p) {
	return o.a == p.a && o.b == p.b && o.g == p.g && o.r == p.r;
}

void Game::render() {
	SDL_RenderClear(_renderer);
	TextureManager::setDrawColor(RENDER_COLOR);
	manager.render();
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