#include "Game.h"
//#include "FallingPlatform.h"
//#include "KillBlock.h"
//#include "WinBlock.h"
//#include "GameWorld.h"
#include "ECS/Components.h"

#define FPS 60
#define WINDOW_TITLE "Protegon"
#define WINDOW_X SDL_WINDOWPOS_CENTERED
#define WINDOW_Y SDL_WINDOWPOS_CENTERED
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_FLAGS SDL_WINDOW_SHOWN//SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN

Game* Game::instance = nullptr;
SDL_Window* Game::window = nullptr;
SDL_Renderer* Game::renderer = nullptr;
bool Game::running = false;
bool Game::bulletTime = false;
//std::vector<Entity*> Game::entities;
int Game::attempts = 1;

Manager manager;
Entity& player(manager.addEntity());
Entity& box(manager.addEntity());

void Game::init() {
	if (initSDL()) {
		running = true;
		//cycle = 0;
		//TextureManager::getInstance();
		//InputHandler::getInstance();
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
		player.add<TransformComponent>(Vec2D(100, 100));
		player.printComponents();
		player.add<SizeComponent>(Vec2D(32, 32));
		player.printComponents();
		player.add<AABBComponent>(AABB(0, 0, 64, 64)); // 1 = size, 2 = transform, 4 = aabb
		player.printComponents();
		player.add<HitboxComponent>(AABB(0, 0, 32, 32)); // 1 = size, 2 = transform, 3 = hitbox
		player.printComponents();
		player.add<SpriteComponent>("./resources/textures/player.png", AABB(0, 0, 16, 16)); // 5 = sprite
		player.printComponents();
		player.add<DrawComponent>(); // 6 = draw
		player.printComponents();
		//box.add<SizeComponent>(Vec2D(64, 64));
		//box.add<SpriteComponent>("./resources/textures/enemy.png", AABB(0, 0, 16, 16));
		//box.add<DrawComponent>();
		player.add<MotionComponent>(Vec2D(1, 1)); // 7 = motion
		player.printComponents();
		player.add<PhysicsComponent>(); // 8 = physics
		player.printComponents();
	}
}

bool Game::initSDL() {
	if (SDL_Init(SDL_INIT_EVERYTHING) != 0) { // failure
		std::cout << "SDL failed to init" << std::endl;
	}
	window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (window) {
		renderer = SDL_CreateRenderer(window, -1, 0);
		if (renderer) {
			//std::cout << "SDL window and renderer init successful" << std::endl;
			return true;
		} else {
			std::cout << "SDL renderer failed to init" << std::endl;
		}
	} else {
		std::cout << "SDL window failed to init" << std::endl;
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
	manager.refresh();
	manager.update();
	//std::cout << player.get<TransformComponent>().getPosition() << std::endl;
	//std::cout << player.get<SizeComponent>().getSize() << ",";//<< std::endl;
	//std::cout << player.get<MotionComponent>().getVelocity() << std::endl;
	//std::string title = "attempts: " + std::to_string(attempts) + ", " + LevelController::getCurrentLevel()->getName();
	//SDL_SetWindowTitle(window, title.c_str());
	//InputHandler::update();
	//for (Entity* e : LevelController::getCurrentLevel()->dynamics) {
	//	e->update();
	//}
	//player->update();
	//player->projectileLifeCheck();
	//for (Bullet* b : player->getProjectiles()) {
	//	b->update();
	//}
	//camera->update();
	//LevelController::getCurrentLevel()->update();
}

void Game::render() {
	SDL_RenderClear(renderer);
	SDL_SetRenderDrawColor(Game::getRenderer(), DEFAULT_RENDER_COLOR.r, DEFAULT_RENDER_COLOR.g, DEFAULT_RENDER_COLOR.b, DEFAULT_RENDER_COLOR.a);
	manager.draw();
	SDL_RenderPresent(renderer); // display
	////SDL_SetRenderDrawColor(renderer, player->getColor().r, player->getColor().g, player->getColor().b, player->getColor().a);
	//TextureManager::draw("player", ((player->getHitbox() + camera->getPosition()) * camera->getScale()), 0.0f, SDL_RendererFlip(player->getDirection()));
	//for (Bullet* b : player->getProjectiles()) {
	//	if (b) {
	//		SDL_SetRenderDrawColor(renderer, b->getColor().r, b->getColor().g, b->getColor().b, b->getColor().a);
	//		SDL_RenderFillRect(renderer, &((b->getHitbox() + camera->getPosition()) * camera->getScale()).AABBtoRect());
	//	}
	//	//SDL_RenderDrawRect(renderer, rect);
	//}
	//for (Entity* e : entities) {
	//	SDL_SetRenderDrawColor(renderer, e->getColor().r, e->getColor().g, e->getColor().b, e->getColor().a);
	//	SDL_RenderDrawRect(renderer, &((e->getHitbox() + camera->getPosition()) * camera->getScale()).AABBtoRect());
	//}
	//for (Entity* e : LevelController::getCurrentLevel()->drawables) {
	//	SDL_SetRenderDrawColor(renderer, e->getColor().r, e->getColor().g, e->getColor().b, e->getColor().a);
	//	SDL_RenderDrawRect(renderer, &((e->getHitbox() + camera->getPosition()) * camera->getScale()).AABBtoRect());
	//}
	//SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
	//if (bulletTime) {
	//	SDL_Delay(2000);
	//}
	//SDL_Delay(0);
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
	SDL_RenderClear(renderer);
	//LevelController::getCurrentLevel()->reset();
	//player->reset();
	//camera->reset();
}

void Game::clean() {
	SDL_DestroyWindow(window);
	SDL_DestroyRenderer(renderer);
	//Quit SDL subsystems
	//IMG_Quit();
	SDL_Quit();
}

void Game::quit() {
	running = false;
}