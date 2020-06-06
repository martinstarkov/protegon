
#include "Game.h"
//#include "FallingPlatform.h"
//#include "KillBlock.h"
//#include "WinBlock.h"
//#include "GameWorld.h"
#include "ECS/Manager.h"
#include "InputHandler.h"
#include "TextureManager.h"

#define FPS 60
#define WINDOW_TITLE "Protegon"
#define WINDOW_X SDL_WINDOWPOS_CENTERED
#define WINDOW_Y SDL_WINDOWPOS_CENTERED
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define WINDOW_FLAGS SDL_WINDOW_SHOWN//SDL_WINDOW_RESIZABLE | SDL_WINDOW_SHOWN

std::unique_ptr<Game> Game::_instance = nullptr;
SDL_Window* Game::_window = nullptr;
SDL_Renderer* Game::_renderer = nullptr;
bool Game::_running = false;

//Manager manager;
//Entity& player(manager.addEntity());

#define DRAG 0.1f

//#define MOVEMENT_MASK (COMPONENT_DISPLACEMENT | COMPONENT_VELOCITY)
//#define RENDER_MASK (COMPONENT_DISPLACEMENT | COMPONENT_APPEARANCE)

Manager manager;
//MovementSystem ms(manager, MOVEMENT_MASK);
//RenderSystem rs(manager, RENDER_MASK);

Entity* tree1;
Entity* tree2;
Entity* tree3;
Entity* tree4;
Entity* box1;
Entity* box2;
Entity* box3;
Entity* box4;
Entity* ghost1;
Entity* ghost2;
Entity* ghost3;
Entity* ghost4;

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
		//cycle = 0;
		TextureManager::getInstance();
		InputHandler::getInstance();
		manager.init();
		LOG_("Manager : ");
		AllocationMetrics::printMemoryUsage();

		tree1 = manager.createTree(30, 30);
		tree2 = manager.createTree(30, 30 * 3);
		tree3 = manager.createTree(30, 30 * 5);
		tree4 = manager.createTree(30, 30 * 7);

		box1 = manager.createBox(30 * 2, 30);
		box2 = manager.createBox(30 * 4, 30);
		box3 = manager.createBox(30 * 6, 30);
		box4 = manager.createBox(30 * 8, 30);

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
		std::cout << "SDL failed to init" << std::endl;
	}
	_window = SDL_CreateWindow(WINDOW_TITLE, WINDOW_X, WINDOW_Y, WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_FLAGS);
	if (_window) {
		_renderer = SDL_CreateRenderer(_window, -1, 0);
		if (_renderer) {
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
	InputHandler::update();
	//printf("%.2fs : ", (float)SDL_GetTicks() / 1000.0f);
	manager.updateSystems();

	//for (auto e : manager.getGroup(Groups::dynamics)) {
	//	e->get<MotionComponent>()->addVelocity(e->get<MotionComponent>()->getAcceleration());
	//	e->get<MotionComponent>()->setVelocity(e->get<MotionComponent>()->getVelocity() * (1.0f - DRAG));
	//	for (auto c : e->getComponents<TransformComponent>()) {
	//		c->addPosition(e->get<MotionComponent>()->getVelocity());
	//	}
	//	e->get<CollisionComponent>()->setColliding(false);
	//	for (auto h : manager.getGroup(Groups::hitboxes)) {
	//		if (e != h) { // do not check with own hitbox
	//			if (e->get<MotionComponent>()->getVelocity()) { // non-zero velocity
	//				if (!h->has<MotionComponent>()) { // dynamic-static check
	//					Vec2D penetration = e->get<HitboxComponent>()->getAABB().colliding(h->get<HitboxComponent>()->getAABB(), e->get<MotionComponent>()->getVelocity());
	//					if (penetration) {
	//						//std::cout << "dynamic-static" << std::endl;
	//						e->get<TransformComponent>()->addPosition(-penetration);
	//						if (penetration.x) { // set x-velocity to 0
	//							//e->get<MotionComponent>()->setAcceleration(Vec2D(0.0f, e->get<MotionComponent>()->getAcceleration().y));
	//							//e->get<MotionComponent>()->setVelocity(Vec2D(0.0f, e->get<MotionComponent>()->getVelocity().y));
	//						}
	//						if (penetration.y) { // set y-velocity to 0
	//							//e->get<MotionComponent>()->setAcceleration(Vec2D(e->get<MotionComponent>()->getAcceleration().x, 0.0f));
	//							//e->get<MotionComponent>()->setVelocity(Vec2D(e->get<MotionComponent>()->getVelocity().x, 0.0f));
	//						}
	//					}
	//				} else { // dynamic-dynamic
	//					std::cout << "dynamic-dynamic" << std::endl;
	//				}
	//			} else { // zero velocity
	//				if (!h->has<MotionComponent>()) { // static-static check
	//					Vec2D penetration = e->get<HitboxComponent>()->getAABB().colliding(h->get<HitboxComponent>()->getAABB());
	//					if (penetration) {
	//						std::cout << "static-static" << std::endl;
	//						e->get<TransformComponent>()->addPosition(-penetration);
	//					}
	//				} else { // static-dynamic
	//					std::cout << "static-dynamic" << std::endl;
	//				}
	//			}
	//		}
	//	}
	//}
	//std::cout << player.get<AABBComponent>()->getAABB() << ",";//std::endl;
	//std::cout << player.get<HitboxComponent>()->getAABB() << std::endl;
	//std::cout << player.get<MotionComponent>().getVelocity() << std::endl;
	//std::string title = "attempts: " + std::to_string(attempts) + ", " + LevelController::getCurrentLevel()->getName();
	//SDL_SetWindowTitle(window, title.c_str());
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