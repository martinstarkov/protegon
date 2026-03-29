#pragma once

#include <optional>
#include <string>

#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"

namespace ptgn {

class Scene;

struct TopDownPlayerConfig {
	// Movement
	float max_speed{ 0.7f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };
	float max_deceleration{ 20.0f * 60.0f };
	float max_turn_speed{ 60.0f * 60.0f };
	float friction{ 1.0f };

	// Hitboxes
	// TODO: Move to a shared hitbox struct.
	V2_float body_hitbox_size{ 10, 6 };
	V2_float body_hitbox_offset{ 0, 8 };
	V2_float interaction_hitbox_size{ 28, 28 };
	V2_float interaction_hitbox_offset{ 0, 0 };

	// Animation
	// TODO: Move to a shared animation struct.
	/// @brief These three are necessary for animation to work.
	std::optional<V2_uint> animation_frame_count;
	std::optional<std::string> animation_texture_key;
	std::optional<V2_int> animation_frame_size;

	/// @brief Defaults to 1000 if not provided.
	std::optional<milliseconds> animation_duration;

	std::optional<Depth> depth;

	// TODO: Move to a shared sound struct.
	/// @brief Required for sound to play
	std::optional<std::string> walk_sound_key;
	/// @brief Defaults to 1 if not provided.
	std::optional<std::size_t> walk_sound_frequency;
};

Entity CreateTopDownPlayer(Scene& scene, V2_float position, const TopDownPlayerConfig& config = {});

} // namespace ptgn