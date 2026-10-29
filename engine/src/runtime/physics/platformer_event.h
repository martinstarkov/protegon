#pragma once

#include <cstdint>

#include "core/math/vector2.h"
#include "runtime/ecs/entity.h"

namespace ptgn::event {

struct PlayerGrounded {
	Entity ground_entity{};
	V2_float normal{};
};

struct PlayerUngrounded {};

struct PlayerLanded {
	Entity ground_entity{};
	V2_float normal{};
	float impact_speed{ 0.0f };
};

struct PlayerJumpApex {
	V2_float position{};
};

struct PlayerHitCeiling {
	Entity obstacle{};
	V2_float normal{};
};

struct PlayerWallContactStart {
	Entity obstacle{};
	V2_float normal{};
};

struct PlayerWallContactStop {
	Entity obstacle{};
};

enum class PlayerJumpType : std::uint8_t {
	Ground,
	Coyote,
	Air,
};

struct PlayerJump {
	PlayerJumpType type{ PlayerJumpType::Ground };
};

} // namespace ptgn::event
