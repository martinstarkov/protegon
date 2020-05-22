#pragma once
#include "LevelController.h"

class GameWorld {
public:
	static GameWorld* getInstance() {
		if (!instance) {
			instance = new GameWorld();
		}
		return instance;
	}
private:
	GameWorld();
private:
	static GameWorld* instance;
	LevelController* levelController;
};

