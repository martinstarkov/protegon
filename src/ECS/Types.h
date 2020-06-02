#pragma once
#include <cstddef>
#include <map>

class Entity;
using EntityID = std::size_t;
constexpr EntityID INVALID_ENTITY_ID = 0;
using ComponentID = std::size_t;
using Entities = std::map<EntityID, Entity*>;
using UniqueEntities = std::map<EntityID, std::unique_ptr<Entity>>;