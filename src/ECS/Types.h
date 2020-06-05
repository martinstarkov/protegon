#pragma once

#include "../common.h"

#include <vector>
#include <map>

class Entity;

using EntityID = unsigned int;
using ComponentID = unsigned int;
using Signature = std::vector<ComponentID>;
using Entities = std::map<EntityID, Entity*>;