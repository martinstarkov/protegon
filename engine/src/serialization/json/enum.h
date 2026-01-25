#pragma once

#include <nlohmann/json.hpp>

#define PTGN_SERIALIZE_ENUM(EnumType, ...) NLOHMANN_JSON_SERIALIZE_ENUM(EnumType, __VA_ARGS__)