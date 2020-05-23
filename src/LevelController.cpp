#include "LevelController.h"
#include "Game.h"

LevelController* LevelController::instance = nullptr;
Level* LevelController::currentLevel = nullptr;
std::map<int, Level*> LevelController::levels;

Level* LevelController::getLevel(int id) {
	return (*levels.find(id)).second;
}

Level* LevelController::getCurrentLevel() {
	return currentLevel;
}

bool LevelController::changeCurrentLevel(int amount) {
	auto it = levels.find(currentLevel->getId() + amount);
	if (it != levels.end()) {
		if ((*it).second) {
			currentLevel = (*it).second;
			return true;
		}
	} else if (currentLevel->getId() + amount < 0) {
		currentLevel = getLevel(0);
	}
	return false;
}

void LevelController::setCurrentLevel(Level* level) {
	std::string name = level->getName();
	if (levels.find(level->getId()) != levels.end()) {
		Game::getInstance()->reset();
		//std::cout << "Setting '" << name << "' as current level" << std::endl;
		currentLevel = level;
	} else {
		//std::cout << "Level '" << name << "' not loaded" << std::endl;
	}
}

void LevelController::setCurrentLevel(int id) {
	if (levels.find(id) != levels.end()) {
		currentLevel = (*levels.find(id)).second;
		Game::getInstance()->reset();
		//std::cout << "Setting '" << currentLevel->getName() << "' as current level" << std::endl;
	} else {
		//std::cout << "Level '" << currentLevel->getName() << "' not loaded" << std::endl;
	}
}

void LevelController::loadLevel(Level* level) {
	int id = level->getId();
	std::string name = level->getName();
	auto it = levels.find(id);
	if (it != levels.end()) {
		//std::cout << "Replacing level '" << (*it).second->getName() << "' with '" << name << "'" << std::endl;
		levels[id] = level;
	} else {
		//std::cout << "Level '" << name << "' loaded" << std::endl;
		levels.insert({ id, level });
		if (!currentLevel) { // initialize current level if it's the first loaded level
			currentLevel = level;
		}
	}
}
