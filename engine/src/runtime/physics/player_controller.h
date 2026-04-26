#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "core/event/event.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/draw.h"
#include "runtime/scripting/script.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;

namespace impl {

struct TopDownMovementScript : public Script {
	void OnEvent(Event d) override;

	void OnMoveStart();

	void OnMoveStop();

	void OnDirectionChange();
};

struct TopDownAnimationRepeat : public Script {
	TopDownAnimationRepeat() = default;

	TopDownAnimationRepeat(std::size_t walk_frequency, std::string_view walk_sound);

	std::size_t walk_sound_frequency{ 1 };
	std::string_view walk_sound_key;

	void OnEvent(Event d) override;

	void OnAnimationFrameChange();
};

} // namespace impl

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

	PTGN_SERIALIZE(
		TopDownPlayerConfig, max_speed, max_acceleration, max_deceleration, max_turn_speed,
		friction, body_hitbox_size, body_hitbox_offset, interaction_hitbox_size,
		interaction_hitbox_offset, animation_frame_count, animation_texture_key,
		animation_frame_size, animation_duration, depth, walk_sound_key, walk_sound_frequency
	)
};

Entity CreateTopDownPlayer(Scene& scene, V2_float position, const TopDownPlayerConfig& config = {});

} // namespace ptgn