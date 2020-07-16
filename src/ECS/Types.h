#pragma once

#include "../common.h"

#include "../Serialization.h"

#include <vector>
#include <set>
#include <map>
#include <string>

class BaseComponent;

using EntityID = unsigned int;
using ComponentID = unsigned int;
using SystemID = unsigned int;
using ComponentName = std::string;
using Signature = std::vector<ComponentID>;
using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;