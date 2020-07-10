#pragma once

#include "../common.h"

#include "../Serialization.h"

#include <vector>
#include <set>
#include <map>

template <typename ...Ts> void swallow(Ts&&... args) {} // wrapper allows calling methods on template pack with void return

using EntityID = unsigned int;
using ComponentID = unsigned int;
using Signature = std::vector<ComponentID>;