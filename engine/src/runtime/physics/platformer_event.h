#pragma once

#include <cstdint>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn::event {

struct PlayerGrounded {
	Entity ground_entity;
	V2_float normal;
};

struct PlayerUngrounded {};

enum class PlayerJumpType : std::uint8_t {
	Ground,
	Coyote,
	Air,
};

struct PlayerJump {
	PlayerJumpType type{ PlayerJumpType::Ground };
};

} // namespace ptgn::event
