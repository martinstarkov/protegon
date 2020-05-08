#include "Game.h"

Game* Game::instance = nullptr;
SDL_Window* Game::window = nullptr;
SDL_Renderer* Game::renderer = nullptr;
bool Game::running = false;
std::vector<Entity*> Game::entities;
std::vector<AABB> Game::broadphase;

Game::Game() {
	tm = TextureManager::getInstance();
	ih = InputHandler::getInstance();
	player = Player::getInstance();
	Entity* box1 = new Entity(AABB(100, 400, 400, 64));
	Entity* box2 = new Entity(AABB(200, 300, 32, 100));
	Entity* box3 = new Entity(AABB(264, 300, 32, 100));
	Entity* box4 = new Entity(AABB(350, 200, 100, 32));
	Entity* box5 = new Entity(AABB(550, 480, 100, 32));
	entities.push_back(box1);
	entities.push_back(box2);
	entities.push_back(box3);
	entities.push_back(box4);
	entities.push_back(box5);
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
	instructions();
}

void Game::instructions() {
	std::cout << "Press 'r' to reset block position" << std::endl;
}

void Game::update() {
	ih->update();
	player->update();
}

void Game::render() {
	SDL_RenderClear(renderer); // clear screen
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_SetRenderDrawColor(renderer, 255, 50, 100, 255);
	SDL_RenderDrawRect(renderer, player->getHitbox().AABBtoRect());
	SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
	for (auto entity : entities) {
		SDL_RenderDrawRect(renderer, entity->getHitbox().AABBtoRect());
	}
	SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
	for (auto aabb : broadphase) {
		//SDL_RenderDrawRect(renderer, aabb.AABBtoRect());
	}
	SDL_SetRenderDrawColor(renderer, 0, 255, 255, 255);
	//SDL_RenderDrawRect(renderer, player->oldHitbox.AABBtoRect());
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderPresent(renderer); // display
	broadphase.clear();
	SDL_Delay(0);
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