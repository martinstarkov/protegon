#pragma once
#include <string>
#include <vector>
#include <tuple>
#include "Entity.h"

// Requires https://github.com/nlohmann/json package installation
#include <nlohmann/json.hpp>

using namespace nlohmann; // the one time I'll use a namespace ;)

class Level {
public:
	Level(std::string path);
	std::string getName() {
		return name;
	}
	int getId() {
		return id;
	}
	Vec2D getSize() {
		return size;
	}
	Vec2D getTileSize() {
		return tileSize;
	}
	Vec2D getSpawn() {
		return spawn;
	}
	void setSpawn(Vec2D newSpawn) {
		spawn = newSpawn;
	}
	Entity* getObject(Vec2D tilePosition);
	void setObject(int id = -1, Vec2D tilePosition = Vec2D(), Vec2D size = Vec2D());
	void deleteObject(Vec2D tilePosition);
	void reset();
public:
	std::vector<Entity*> statics;
	std::vector<Entity*> dynamics;
	std::vector<Entity*> drawables;
	std::vector<Entity*> interactables;
private:
	void readJson();
	void readGrid();
	Entity* createEntity(int id, Vec2D tilePosition, Vec2D size);
private:
	json j;
	std::string name;
	int id;
	int rows, columns;
	Vec2D size, tileSize;
	Vec2D spawn;
	std::map<int, std::map<int, Entity*>> data;
};

