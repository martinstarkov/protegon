#include "runtime/physics/player_controller.h"

#include <chrono>
#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/event/event.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/animation_event.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/movement_event.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scripting/script.h"

namespace ptgn {

namespace impl {

void TopDownMovementScript::OnEvent(Event event) {
	event.Dispatch<event::PlayerMoveStart>(&TopDownMovementScript::OnMoveStart, this);
	event.Dispatch<event::PlayerMoveStop>(&TopDownMovementScript::OnMoveStop, this);
	event.Dispatch<event::PlayerMoveDirectionChange>(
		&TopDownMovementScript::OnDirectionChange, this
	);
}

void TopDownMovementScript::OnMoveStart() {
	auto active{ Target().Get<AnimationMap>().GetActive() };
	PTGN_ASSERT(active.has_value());
	active.value().Start(false);
}

void TopDownMovementScript::OnMoveStop() {
	auto active{ Target().Get<AnimationMap>().GetActive() };
	PTGN_ASSERT(active.has_value());
	active.value().Reset();
}

void TopDownMovementScript::OnDirectionChange() {
	auto& a{ Target().Get<AnimationMap>() };
	auto dir{ Target().Get<TopDownMovement>().GetDirection() };
	auto prev_active{ a.GetActive() };
	PTGN_ASSERT(prev_active.has_value());
	bool active_changed{ false };

	switch (dir) {
		using enum ptgn::MoveDirection;
		case Down:		active_changed = a.SetActive("down"); break;
		case Up:		active_changed = a.SetActive("up"); break;
		case Left:		[[fallthrough]];
		case DownLeft:	[[fallthrough]];
		case UpLeft:	[[fallthrough]];
		case UpRight:	[[fallthrough]];
		case DownRight: [[fallthrough]];
		case Right:		active_changed = a.SetActive("right"); break;
		default:		break;
	}
	if (active_changed) {
		prev_active.value().Reset();
	}
	auto current_active{ a.GetActive() };
	PTGN_ASSERT(current_active.has_value());
	current_active.value().Start(false);
}

TopDownAnimationRepeat::TopDownAnimationRepeat(
	std::size_t walk_frequency, std::string_view walk_sound
) :
	walk_sound_frequency{ walk_frequency }, walk_sound_key{ walk_sound } {}

void TopDownAnimationRepeat::OnEvent(Event d) {
	d.Dispatch<event::AnimationFrameChange>(&TopDownAnimationRepeat::OnAnimationFrameChange, this);
}

void TopDownAnimationRepeat::OnAnimationFrameChange() {
	auto frame{ Animation{ Target() }.GetCurrentFrame() };
	if (frame % walk_sound_frequency == 0) {
		Target().GetScene().ctx().audio.Play(walk_sound_key);
	}
}

} // namespace impl

Entity CreateTopDownPlayer(Scene& scene, Transform transform, const TopDownPlayerConfig& config) {
	auto player{ scene.CreateEntity() };

	player.Add<Tag>("Top Down Player");
	player.Add<Transform>(transform);
	player.Add<RigidBody>();

	if (config.depth.has_value()) {
		SetDepth(player, config.depth.value());
	}

	auto body_hitbox{ scene.CreateEntity() };
	body_hitbox.Add<Tag>("Body Hitbox");
	body_hitbox.Add<Collider>(Rect{ config.body_hitbox_size });
	SetPosition(body_hitbox, config.body_hitbox_offset);
	body_hitbox.Add<RigidBody>();

	auto interaction_hitbox{ scene.CreateEntity() };
	interaction_hitbox.Add<Tag>("Interaction Hitbox");
	auto& interaction_collider{ interaction_hitbox.Add<Collider>(Rect{
		config.interaction_hitbox_size }) };
	interaction_collider.SetCollisionMode(CollisionMode::Overlap);
	SetPosition(interaction_hitbox, {});

	AddChild(player, body_hitbox, "body");
	AddChild(player, interaction_hitbox, "interaction");

	auto& movement{ player.Add<TopDownMovement>() };
	movement.max_speed		  = config.max_speed;
	movement.max_acceleration = config.max_acceleration;
	movement.max_deceleration = config.max_deceleration;
	movement.max_turn_speed	  = config.max_turn_speed;
	movement.friction		  = config.friction;

	if (config.animation_texture_key && config.animation_frame_count.has_value()) {
		Transform animation_transform;
		milliseconds duration{ config.animation_duration.value_or(1000ms) };

		AnimationMap anim_map{ CreateAnimationMap(scene) };
		auto anim0{ CreateAnimation(
			scene, animation_transform, config.animation_texture_key,
			{ .frame_count = config.animation_frame_count.value().x,
			  .duration	   = duration,
			  .frame_size  = config.animation_frame_size.value_or(V2_int{}) }
		) };
		anim0.Add<Tag>("Down Animation");
		auto a0 = anim_map.Add("down", anim0);
		anim_map.SetActive("down");
		auto anim1{ CreateAnimation(
			scene, animation_transform, config.animation_texture_key,
			{ config.animation_frame_count.value().x, duration,
			  config.animation_frame_size.value_or(V2_int{}), std::nullopt,
			  V2_float{ 0, config.animation_frame_size.value().y } }
		) };
		anim1.Add<Tag>("Right Animation");
		auto a1 = anim_map.Add("right", anim1);
		auto anim2{ CreateAnimation(
			scene, animation_transform, config.animation_texture_key,
			{ config.animation_frame_count.value().x, duration,
			  config.animation_frame_size.value_or(V2_int{}), std::nullopt,
			  V2_float{ 0, 2 * config.animation_frame_size.value().y } }
		) };
		anim2.Add<Tag>("Up Animation");
		auto a2 = anim_map.Add("up", anim2);

		SetParent(anim_map, player);

		if (config.walk_sound_key) {
			PTGN_ASSERT(
				impl::AssetAccessor{ scene.ctx().asset }.Has<Audio>(config.walk_sound_key),
				"Walk sound not found"
			);
			auto frequency{ config.walk_sound_frequency.value_or(1) };

			AddScript<impl::TopDownAnimationRepeat>(a0, frequency, config.walk_sound_key);
			AddScript<impl::TopDownAnimationRepeat>(a1, frequency, config.walk_sound_key);
			AddScript<impl::TopDownAnimationRepeat>(a2, frequency, config.walk_sound_key);
		}

		AddScript<impl::TopDownMovementScript>(player);
	}

	return player;
}

} // namespace ptgn