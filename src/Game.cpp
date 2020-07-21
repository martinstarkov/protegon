#include "Game.h"
#include "ECS/Manager.h"
#include "ECS/EntityHandle.h"
#include "InputHandler.h"
#include "TextureManager.h"

std::unique_ptr<Game> Game::_instance = nullptr;
SDL_Window* Game::_window = nullptr;
SDL_Renderer* Game::_renderer = nullptr;
bool Game::_running = false;

SDL_Event event;

Manager manager;

EntityHandle tree1;
EntityHandle box1;
EntityHandle player;

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

	tree1 = EntityHandle(manager.createTree(Vec2D(30)), &manager);
	box1 = EntityHandle(manager.createBox(Vec2D(30 * 2 * 2, 30)), &manager);
	player = EntityHandle(manager.createPlayer(Vec2D(30 * 10, 30)), &manager);

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
}

void Game::render() {
	SDL_RenderClear(_renderer);
	SDL_SetRenderDrawColor(getRenderer(), DEFAULT_RENDER_COLOR.r, DEFAULT_RENDER_COLOR.g, DEFAULT_RENDER_COLOR.b, DEFAULT_RENDER_COLOR.a);
	manager.render();
	SDL_RenderPresent(_renderer);
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