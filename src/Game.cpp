#include "Game.h"
#include "FallingPlatform.h"
#include "KillBlock.h"
#include "WinBlock.h"

Game* Game::instance = nullptr;
SDL_Window* Game::window = nullptr;
SDL_Renderer* Game::renderer = nullptr;
bool Game::running = false;
std::vector<Entity*> Game::entities;
std::vector<Entity*> Game::entityObjects;
std::vector<AABB> Game::broadphase;
Uint32 Game::time;
Uint32 Game::previousTime;
bool Game::bulletTime = false;

Game::Game() {
	cycle = 0;
	tm = TextureManager::getInstance();
	ih = InputHandler::getInstance();
	player = Player::getInstance();
	camera = Camera::getInstance();
	//entities.push_back(new Entity(AABB(32, 32, 32, 32)));
	//entities.push_back(new Entity(AABB(96, 96, 32, 32)));
	//entities.push_back(new Entity(AABB(160, 160, 32, 32)));
	//entities.push_back(new Entity(AABB(224, 224, 32, 32)));
	//entities.push_back(new Entity(AABB(288, 288, 32, 32)));
	//entities.push_back(new Entity(AABB(352, 224, 32, 32)));
	//entities.push_back(new Entity(AABB(416, 160, 32, 32)));
	//entities.push_back(new Entity(AABB(480, 96, 32, 32)));
	//entities.push_back(new Entity(AABB(544, 32, 32, 32)));
	//entities.push_back(new FallingPlatform(AABB(600, 550, 32, 32), 1));
	//entities.push_back(new FallingPlatform(AABB(650, 460, 32, 32), 0.8f));
	//entities.push_back(new FallingPlatform(AABB(700, 380, 32, 32), 0.5f));
	//entities.push_back(new FallingPlatform(AABB(550, 360, 32, 32), 0.5f));
	//entities.push_back(new FallingPlatform(AABB(150, 400, 350, 32), 5));
	//entities.push_back(new KillBlock(AABB(80, 390, 32, 32)));
	//entities.push_back(new WinBlock(AABB(20, 380, 32, 32)));
	entities.push_back(new Entity(AABB(128, 0, 128, 128)));
	entities.push_back(new Entity(AABB(128, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128, 128 * 2, 128, 128)));
	entities.push_back(new Entity(AABB(128, 128 * 3, 128, 128)));
	entities.push_back(new Entity(AABB(128, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 2, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 3, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 4, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 5, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 6, 128, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 6, 128 * 2, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 7, 128 * 3, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 7, 128 * 4, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 7, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 6, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 5, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 4, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 3, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(128 * 2, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(0, 128 * 5, 128, 128)));
	entities.push_back(new Entity(AABB(0, 128 * 4, 128, 128)));
	entities.push_back(new Entity(AABB(0, 128 * 3, 128, 128)));
	for (Entity* entity : entities) {
		entityObjects.push_back(entity);
	}
	entities.push_back(player);
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
	cycle = 0;
	instructions();
}

void Game::instructions() {
	std::cout << "'w', 'a', 's', 'd' to move" << std::endl;
	std::cout << "'r' to reset game" << std::endl;
}

void Game::update() {
	time = SDL_GetTicks();
	ih->update();
	for (Entity* entity : entityObjects) {
		entity->update();
	}
	player->update();
	camera->update();
	previousTime = time;
}

void Game::render() {
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_SetRenderDrawColor(renderer, player->getColor().r, player->getColor().g, player->getColor().b, player->getColor().a);
	SDL_RenderDrawRect(renderer, ((player->getHitbox() + camera->getPosition()) * camera->getScale()).AABBtoRect());
	for (Entity* entity : entityObjects) {
		SDL_SetRenderDrawColor(renderer, entity->getColor().r, entity->getColor().g, entity->getColor().b, entity->getColor().a);
		SDL_RenderDrawRect(renderer, ((entity->getHitbox() + camera->getPosition()) * camera->getScale()).AABBtoRect());
	}
	//SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	//Vec2D point = Vec2D(200, 200);
	//SDL_RenderDrawPoint(renderer, int(point.x), int(point.y));
	//SDL_Rect rect = { b.pos.x + point.x, b.pos.y + point.y, b.size.x, b.size.y };
	//SDL_SetRenderDrawColor(renderer, 0, 120, 0, 255);
	//for (AABB b : broadphase) {
	//	SDL_RenderDrawRect(renderer, b.AABBtoRect());
	//}
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderPresent(renderer); // display
	broadphase.clear();
	if (bulletTime) {
		SDL_Delay(2000);
	}
	SDL_Delay(0);
}

void Game::loop() {
	const int fDelay = 1000 / FPS;
	Uint32 fStart;
	int fTime;
	while (running) {
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

void Game::reset() {
	std::cout << "Resetting game..." << std::endl;
	for (Entity* entity : Game::entities) {
		entity->reset();
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