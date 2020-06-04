#pragma once

#include "../common.h"

#include <vector>
#include <map>

class Entity;

using EntityID = std::size_t;
using ComponentID = std::size_t;
using Signature = std::vector<ComponentID>;
using Entities = std::map<EntityID, Entity*>;