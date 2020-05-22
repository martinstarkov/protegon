#include "GameWorld.h"

GameWorld* GameWorld::instance = nullptr;

GameWorld::GameWorld() {
	levelController = LevelController::getInstance();
}
