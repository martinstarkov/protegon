#pragma once

#include "../common.h"

#include "../Serialization.h"

#include <vector>
#include <set>
#include <map>
#include <string>
#include <memory>

class BaseComponent;

using EntityID = std::size_t; // possibly upgrade to unsigned long int later
using ComponentID = std::size_t;
using SystemID = std::size_t;
using AnimationName = std::string;
using ComponentName = std::string;
using EntitySet = std::set<EntityID>;
using Signature = std::vector<ComponentID>;
using ComponentMap = std::map<ComponentID, std::unique_ptr<BaseComponent>>;