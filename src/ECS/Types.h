#pragma once

#include "../common.h"

#include "../Serialization.h"

#include <vector>
#include <set>
#include <map>
#include <string>

class BaseComponent;

using EntityID = std::size_t;
using ComponentID = std::size_t;
using SystemID = std::size_t;
using ComponentName = std::string;
using EntitySet = std::set<EntityID>;
using Signature = std::vector<ComponentID>;
using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;