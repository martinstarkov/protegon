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
	//entities.push_back(new Entity(AABB(100, 520, 32, 32)));
	//entities.push_back(new Entity(AABB(32, 32, 32, 32)));
	//entities.push_back(new Entity(AABB(96, 96, 32, 32)));
	//entities.push_back(new Entity(AABB(160, 160, 32, 32)));
	//entities.push_back(new Entity(AABB(224, 224, 32, 32)));
	//entities.push_back(new Entity(AABB(288, 288, 32, 32)));
	//entities.push_back(new Entity(AABB(352, 224, 32, 32)));
	//entities.push_back(new Entity(AABB(416, 160, 32, 32)));
	//entities.push_back(new Entity(AABB(480, 96, 32, 32)));
	//entities.push_back(new Entity(AABB(544, 32, 32, 32)));
	//entities.push_back(new Entity(AABB(600, 550, 32, 32)));
	//entities.push_back(new Entity(AABB(650, 460, 32, 32)));
	//entities.push_back(new Entity(AABB(700, 380, 32, 32)));
	//entities.push_back(new Entity(AABB(550, 360, 32, 32)));
	//entities.push_back(new Entity(AABB(150, 400, 350, 32)));
	Entity* box1 = new Entity(AABB(10 + 700 - 1, 10 + 128, 128, 128));
	Entity* box2 = new Entity(AABB(10 + 700, 10 + 128 + 128, 128, 128));
	Entity* box3 = new Entity(AABB(10 + 700, 10 + 128 + 128 + 128, 128, 128));
	Entity* box4 = new Entity(AABB(10 + 700 - 128, 10 + 128 + 128 + 128, 128, 128));
	Entity* box5 = new Entity(AABB(10 + 700 - 128 - 128, 10 + 128 + 128 + 128, 128, 128));
	box1->setId(1);
	box2->setId(2);
	box3->setId(3);
	box4->setId(-2);
	box5->setId(-1);
	entities.push_back(box1);
	entities.push_back(box2);
	entities.push_back(box3);
	entities.push_back(box4);
	entities.push_back(box5);
	//Entity* rightBox = new Entity(AABB(230, 550, 32, 32), {}, {}, {}, false, 15, { 0, 120, 0, 255 });
	//Entity* platform = new Entity(AABB(80, 560, 300, 32), {}, {}, {}, false, 10, { 0, 0, 0, 255 });
	//Entity* leftBox = new Entity(AABB(26, 550, 32, 32), {}, {}, {}, false, 5, { 120, 0, 0, 255 });
	//Entity* rock = new Entity(AABB(380, 560, 32, 32), {}, {}, {}, false, 20, { 120, 120, 0, 255 });
	//entities.push_back(platform);
	//entities.push_back(rightBox);
	//entities.push_back(leftBox);
	//entities.push_back(rock);
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
	previousTime = time;
}

void Game::render() {
	if (!bulletTime) {
		SDL_RenderClear(renderer); // clear screen
	}
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_SetRenderDrawColor(renderer, player->getColor().r, player->getColor().g, player->getColor().b, player->getColor().a);
	SDL_RenderDrawRect(renderer, player->getHitbox().AABBtoRect());
	for (Entity* entity : entityObjects) {
		SDL_SetRenderDrawColor(renderer, entity->getColor().r, entity->getColor().g, entity->getColor().b, entity->getColor().a);
		SDL_RenderDrawRect(renderer, entity->getHitbox().AABBtoRect());
	}
	SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
	Vec2D point = Vec2D(300, 300);
	SDL_RenderDrawPoint(renderer, int(point.x), int(point.y));
	//SDL_Rect rect = { b.pos.x + point.x, b.pos.y + point.y, b.size.x, b.size.y };
	for (AABB b : broadphase) {
		SDL_RenderDrawRect(renderer, b.AABBtoRect());
	}
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	SDL_RenderPresent(renderer); // display
	broadphase.clear();
	if (bulletTime) {
		SDL_Delay(2000);
	}
	SDL_Delay(10);
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