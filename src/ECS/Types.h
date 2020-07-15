#pragma once

#include "../common.h"

#include "../Serialization.h"

#include <vector>
#include <set>
#include <map>

class BaseComponent;

using EntityID = unsigned int;
using ComponentID = unsigned int;
using SystemID = unsigned int;
using Signature = std::vector<ComponentID>;
using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;