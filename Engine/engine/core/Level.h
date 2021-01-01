#pragma once

#include <vector> // std::vector
#include <cstdlib> // std::size_t
#include "utils/Vector2.h"
#include "ecs/ECS.h" // ecs::Entity, ecs::Manager

namespace engine {

class Level {
public:
	Level() = delete;
	Level(ecs::Manager& manager);
	Level(V2_int size, ecs::Manager& manager);
	~Level();
	ecs::Entity GetEntity(V2_int location) const;
	ecs::Entity& GetEntity(V2_int location);
	// Generates a new grid of entities with the given size.
	void SetSize(V2_int new_size);
	V2_int GetSize() const;
private:
	// Destroys all grid entities and resets grid size to 0.
	void Clear();
	std::size_t GetIndex(V2_int location) const;
	ecs::Manager& manager;
	V2_int size;
	std::vector<ecs::Entity> grid;
};

} // namespace engine