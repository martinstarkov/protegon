#pragma once
#include <iostream>
#include <string>
#include <map>
#include "Level.h"

class LevelController {
public:
	static LevelController* getInstance() {
		if (!instance) {
			instance = new LevelController();
		}
		return instance;
	}
public:
	static Level* getLevel(int id);
	static Level* getCurrentLevel();
	static void setCurrentLevel(int id);
	static void setCurrentLevel(Level* level);
	static bool changeCurrentLevel(int amount);
	static void loadLevel(Level* level);
private:
	static LevelController* instance;
	static Level* currentLevel;
	static std::map<int, Level*> levels;
};
