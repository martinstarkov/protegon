
#include "Game.h"
#include "ECS/Manager.h"
#include "InputHandler.h"
#include "TextureManager.h"

#define WINDOW_TITLE "Protegon"
#define WINDOW_X SDL_WINDOWPOS_CENTERED
#define WINDOW_Y SDL_WINDOWPOS_CENTERED
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_FLAGS SDL_WINDOW_SHOWN //SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN

std::unique_ptr<Game> Game::_instance = nullptr;
SDL_Window* Game::_window = nullptr;
SDL_Renderer* Game::_renderer = nullptr;
bool Game::_running = false;

Manager manager;

EntityID tree1;
EntityID tree2;
EntityID tree3;
EntityID tree4;
EntityID box1;
EntityID box2;
EntityID box3;
EntityID box4;
EntityID ghost1;
EntityID ghost2;
EntityID ghost3;
EntityID ghost4;

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
	if (initSDL()) {
		_running = true;
		TextureManager::getInstance();
		InputHandler::getInstance();
		manager.init();
		LOG_("Manager : ");
		AllocationMetrics::printMemoryUsage();

		tree1 = manager.createTree(30, 30);
		tree2 = manager.createTree(30, 30 * 3);
		tree3 = manager.createTree(30, 30 * 5);
		tree4 = manager.createTree(30, 30 * 7);

		box1 = manager.createBox(30 * 2 * 2, 30);
		box4 = manager.createBox(30 * 10, 30);

		ghost1 = manager.createGhost(80, 80);
		ghost2 = manager.createGhost(80 * 2, 80 * 2);
		ghost3 = manager.createGhost(80 * 3, 80 * 3);
		ghost4 = manager.createGhost(80 * 4, 80 * 4);

		manager.refreshSystems();
		//GameWorld::getInstance();
		//LevelController::loadLevel(new Level("./resources/levels/level0.json"));
		//LevelController::loadLevel(new Level("./resources/levels/level1.json"));
		//LevelController::loadLevel(new Level("./resources/levels/level2.json"));
		//LevelController::loadLevel(new Level("./resources/levels/level3.json"));
		//LevelController::loadLevel(new Level("./resources/levels/victory.json"));
		//player = Player::getInstance();
		//player->setPosition(LevelController::getCurrentLevel()->getSpawn());
		//camera = Camera::getInstance();
		//TextureManager::load("player", "./resources/textures/player.png");
		//instructions();
	}
}

bool Game::initSDL() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { // failure
		LOG("SDL failed to init");
	}
	_window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (_window) {
		_renderer = SDL_CreateRenderer(_window, -1, 0);
		if (_renderer) {
			//LOG("SDL window and renderer init successful");
			return true;
		} else {
			LOG("SDL renderer failed to init");
		}
	} else {
		LOG("SDL window failed to init");
	}
	return false;
}

void Game::instructions() {
	std::cout << "(w, a, s, d) -> move" << std::endl;
	std::cout << "(q) -> zoom in, (e) -> zoom out" << std::endl;
	std::cout << "(r) -> restart game to tutorial" << std::endl;
	std::cout << "(c) -> shoot" << std::endl;
}

void Game::update() {
	InputHandler::update();

	manager.updateSystems();
}

void Game::render() {
	SDL_RenderClear(_renderer);

	SDL_SetRenderDrawColor(Game::getRenderer(), DEFAULT_RENDER_COLOR.r, DEFAULT_RENDER_COLOR.g, DEFAULT_RENDER_COLOR.b, DEFAULT_RENDER_COLOR.a);

	assert(manager.getSystem<RenderSystem>() != nullptr);
	manager.getSystem<RenderSystem>()->update();

	SDL_RenderPresent(_renderer);
}

void Game::loop() {
	const int fDelay = 1000 / FPS;
	Uint32 fStart;
	int fTime;
	while (_running) {
		fStart = SDL_GetTicks();
		update();
		render();
		fTime = SDL_GetTicks() - fStart;
		cycle++;
		if (fDelay > fTime) {
			SDL_Delay(fDelay - fTime);
		}
	}
}

void Game::clean() {
	SDL_DestroyWindow(_window);
	SDL_DestroyRenderer(_renderer);
	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Game::quit() {
	_running = false;
}