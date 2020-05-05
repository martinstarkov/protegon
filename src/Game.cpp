#include "Game.h"

Game* Game::instance = nullptr;
SDL_Window* Game::window = nullptr;
SDL_Renderer* Game::renderer = nullptr;
bool Game::running = false;
std::vector<Entity*> Game::entities;

Game::Game() {
	tm = TextureManager::getInstance();
	ih = InputHandler::getInstance();
	player = Player::getInstance();
	Entity* box0 = new Entity(Vec2D(40, 40), Vec2D(120, 200), Vec2D(), Vec2D(), -1);
	Entity* box1 = new Entity(Vec2D(400, 32), Vec2D(100, 400), Vec2D(), Vec2D(), -1);
	Entity* box2 = new Entity(Vec2D(32, 100), Vec2D(200, 300), Vec2D(), Vec2D(), -1);
	//entities.push_back(box0);
	entities.push_back(box1);
	//entities.push_back(box2);
}

void Game::init() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { // failure
		std::cout << "SDL failed to init" << std::endl;
	}
	window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (window) {
		renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			std::cout << "SDL window and renderer init successful" << std::endl;
		} else {
			std::cout << "SDL renderer failed to init" << std::endl;
		}
	} else {
		std::cout << "SDL window failed to init" << std::endl;
	}
	running = true;
}

void Game::update() {
	ih->update();
	player->update();
}

void Game::render() {
	SDL_RenderClear(renderer); // clear screen
	SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
	SDL_RenderDrawRect(renderer, &player->getPosition().Vec2DtoSDLRect(player->getSize()));
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	for (auto entity : entities) {
		SDL_RenderDrawRect(renderer, &entity->getPosition().Vec2DtoSDLRect(entity->getSize()));
	}
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderPresent(renderer); // display
}

void Game::loop() {
	const int FPS = 60;
	const int fDelay = 1000 / FPS;
	Uint32 fStart;
	int fTime;
	while (running) {
		fStart = SDL_GetTicks();
		update();
		render();
		fTime = SDL_GetTicks() - fStart;
		if (fDelay > fTime) {
			SDL_Delay(fDelay - fTime);
		}
	}
}

void Game::clean() {
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	//Quit SDL subsystems
	IMG_Quit();
	SDL_Quit();
}

void Game::quit() {
	running = false;
}