#pragma once

#include <string_view>

#include "core/entity.h"
#include "core/manager.h"
#include "math/vector2.h"
#include "components/common.h"
#include "core/time.h"
#include "rendering/api/origin.h"

namespace ptgn {

struct TopDownPlayerConfig {
	// Movement
	float max_speed{ 0.7f * 60.0f };
	float max_acceleration{ 20.0f * 60.0f };
	float max_deceleration{ 20.0f * 60.0f };
	float max_turn_speed{ 60.0f * 60.0f };
	float friction{ 1.0f };

	// Hitboxes
	V2_float body_hitbox_size{ 10, 6 };
	V2_float body_hitbox_offset{ 0, 8 };
	Origin body_hitbox_origin{ Origin::CenterBottom };
	V2_float interaction_hitbox_size{ 28, 28 };
	V2_float interaction_hitbox_offset{ 0, 0 };
	Origin interaction_hitbox_origin{ Origin::Center };

	// Animation
	V2_uint animation_frame_count{ 4, 3 };
	V2_int animation_frame_size{ 16, 17 };
	milliseconds animation_duration{ 1000 };

	Depth depth{ 1 };

	// Sound
	int walk_sound_frequency{ 2 };

	std::string_view walk_sound_key{ "walk" };
	std::string_view animation_texture_key{ "player_anim" };
};

Entity CreateTopDownPlayer(
	Manager& manager, const V2_float& position, const TopDownPlayerConfig& config = {}
);

} // namespace ptgn