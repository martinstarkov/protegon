#include "Level.h"
#include <fstream>
#include <sstream>
#include "Entities.h"

Level::Level(std::string path) {
	rows = 0; columns = 0; size = Vec2D();
	std::ifstream file(path);
	if (file) {
		file >> j;
		readJson();
		//std::cout << "Level '" << path << "' processed" << std::endl;
	} else {
		std::cout << "Level '" << path << "' NOT FOUND" << std::endl;
	}
}

Entity* Level::createEntity(int id, Vec2D size, Vec2D destination) {
	AABB block = AABB(destination, size);
	Entity* e = nullptr;
	switch (id) {
		case KILL_TILE_ID:
			e = new KillBlock(block);
			statics.push_back(e);
			break;
		case WIN_TILE_ID:
			e = new WinBlock(block);
			statics.push_back(e);
			break;
		case FALLING_TILE_ID:
			e = new FallingPlatform(block);
			dynamics.push_back(e);
			break;
		case UNKNOWN_TILE_ID:
			e = new Entity(block);
			statics.push_back(e);
			break;
		default:
			break;
	}
	if (e) {
		drawables.push_back(e);
	}
	return e;
}

void Level::readGrid() {
	if (j["grid"].size() > 0) {
		json grid = j["grid"];
		rows = grid.size();
		columns = grid[0].size();
		tileSize = Vec2D(16, 16);
		if (!j["tile_size"].is_null()) {
			json s = j["tile_size"];
			if (s.size() > 1) {
				tileSize = Vec2D(s[0].get<int>(), s[1].get<int>());
			}
		}
		size = Vec2D(columns, rows) * tileSize;
		for (int row = 0; row < rows; row++) {
			std::map<int, Entity*> tempColumn;
			for (int column = 0; column < columns; column++) {
				Entity* e = createEntity(grid[row][column].get<int>(), tileSize, Vec2D(column, row) * tileSize);
				if (e) {
					tempColumn.insert({ column, e });
				}
			}
			if (tempColumn.size() > 0) {
				data.insert({ row, tempColumn });
			}
		}
	}
}

void Level::readJson() {
	if (!j["id"].is_null()) {
		id = j["id"].get<int>();
	} else {
		id = -1;
	}
	if (!j["name"].is_null()) {
		name = j["name"].get<std::string>();
	} else {
		name = "unknown";
	}
	readGrid();
	if (!j["spawn"].is_null()) {
		json s = j["spawn"];
		if (s.size() > 1) {
			spawn = Vec2D(s[0].get<int>(), s[1].get<int>()) * tileSize;
		}
	} else {
		spawn = size / 2.0f;
	}
}

Entity* Level::getObject(int x, int y) {
	auto xIt = data.find(x); // row iterator
	if (xIt != data.end()) {
		auto yIt = (*xIt).second.find(y); // column iterator
		if (yIt != (*xIt).second.end()) {
			return (*yIt).second; // entity
		}
	}
	return nullptr;
}