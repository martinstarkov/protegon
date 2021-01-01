#include "Level.h"

namespace engine {

Level::Level(ecs::Manager& manager) : manager{ manager } {}

Level::Level(V2_int size, ecs::Manager& manager) : size{ size }, manager{ manager } {
	SetSize(size);
}

Level::~Level() {
	Clear();
}

ecs::Entity Level::GetEntity(V2_int location) const {
	return grid[GetIndex(location)];
}

ecs::Entity& Level::GetEntity(V2_int location) {
	return grid[GetIndex(location)];
}

void Level::SetSize(V2_int new_size) {
	Clear();
	size = new_size;
	auto count = size.x * size.y;
	// Generate new empty grid.
	grid.resize(static_cast<std::size_t>(count), ecs::null);
	//for (auto i = 0; i < count; ++i) {
	//	grid[i] = manager.CreateEntity();
	//	//LOG("Creating " << i);
	//}
}

V2_int Level::GetSize() const {
	return size;
}

void Level::Clear() {
	auto count = size.x * size.y;
	for (auto i = 0; i < count; ++i) {
		grid[i].Destroy();
	}
}

std::size_t Level::GetIndex(V2_int location) const {
	assert(location.x < size.x && "X coordinate out of range of level grid");
	assert(location.y < size.y && "Y coordinate out of range of level grid");
	auto index = location.x + location.y * size.x;
	assert(index < grid.size() && "Index out of range of level grid");
	return index;
}

} // namespace engine