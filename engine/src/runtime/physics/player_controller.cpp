#include "runtime/physics/player_controller.h"

#include <optional>
#include <string_view>

#include "core/assert.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "core/time/time.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/game_object.h"
#include "core/event/event_dispatcher.h"
#include "runtime/graphics/draw.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/move_direction.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"

namespace ptgn {

Entity CreateTopDownPlayer(Scene& scene, V2_float position, const TopDownPlayerConfig& config) {
	auto player{ scene.CreateEntity() };

	SetPosition(player, position);
	player.Add<RigidBody>();

	if (config.depth.has_value()) {
		SetDepth(player, *config.depth);
	}

	auto body_hitbox{ scene.CreateEntity() };
	body_hitbox.Add<Collider>(Rect{ config.body_hitbox_size });
	SetPosition(body_hitbox, config.body_hitbox_offset);
	body_hitbox.Add<RigidBody>();

	auto interaction_hitbox{ scene.CreateEntity() };
	auto& interaction_collider =
		interaction_hitbox.Add<Collider>(Rect{ config.interaction_hitbox_size });
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

	if (config.animation_texture_key.has_value() && config.animation_frame_count.has_value()) {
		PTGN_ASSERT(
			scene.ctx().asset.HasTexture(*config.animation_texture_key),
			"Cannot create player with animation key which has not been loaded"
		);

		auto texture{ *scene.ctx().asset.GetTexture(*config.animation_texture_key) };
		V2_float anim_position;
		auto duration{ config.animation_duration.value_or(1000ms) };

		AnimationMap anim_map{ player.Add<GameObject<AnimationMap>>(CreateAnimationMap(scene)) };
		auto a0 = anim_map.Add(
			"down", CreateAnimation(
						scene, texture, anim_position,
						{ config.animation_frame_count->x, duration,
						  config.animation_frame_size.value_or(V2_int{}) }
					)
		);
		anim_map.SetActive("down");
		auto a1 = anim_map.Add(
			"right", CreateAnimation(
						 scene, texture, anim_position,
						 { config.animation_frame_count->x, duration,
						   config.animation_frame_size.value_or(V2_int{}), std::nullopt,
						   V2_float{ 0, config.animation_frame_size->y } }
					 )
		);
		auto a2 = anim_map.Add(
			"up", CreateAnimation(
					  scene, texture, anim_position,
					  { config.animation_frame_count->x, duration,
						config.animation_frame_size.value_or(V2_int{}), std::nullopt,
						V2_float{ 0, 2 * config.animation_frame_size->y } }
				  )
		);

		SetParent(a0, player);
		SetParent(a1, player);
		SetParent(a2, player);

		struct AnimationRepeat : public Script {
			AnimationRepeat() = default;

			AnimationRepeat(std::size_t walk_frequency, std::string_view walk_sound) :
				walk_sound_frequency{ walk_frequency }, walk_sound_key{ walk_sound } {}

			std::size_t walk_sound_frequency{ 1 };
			std::string_view walk_sound_key;

			void OnEvent(EventDispatcher d) override {
				d.Dispatch<event::AnimationFrameChange>(
					&AnimationRepeat::OnAnimationFrameChange, this
				);
			}

			void OnAnimationFrameChange() {
				auto frame{ Animation{ entity }.GetCurrentFrame() };
				if (frame % walk_sound_frequency == 0) {
					entity.GetScene().ctx().audio.Play(walk_sound_key);
				}
			}
		};

		if (config.walk_sound_key.has_value()) {
			PTGN_ASSERT(scene.ctx().asset.HasAudio(*config.walk_sound_key));
			auto frequency{ config.walk_sound_frequency.value_or(1) };

			AddScript<AnimationRepeat>(a0, frequency, *config.walk_sound_key);
			AddScript<AnimationRepeat>(a1, frequency, *config.walk_sound_key);
			AddScript<AnimationRepeat>(a2, frequency, *config.walk_sound_key);
		}

		struct MovementScript : public Script {
			void OnEvent(EventDispatcher d) override {
				d.Dispatch<event::PlayerMoveStart>(&MovementScript::OnMoveStart, this);
				d.Dispatch<event::PlayerMoveStop>(&MovementScript::OnMoveStop, this);
				d.Dispatch<event::PlayerMoveDirectionChange>(
					&MovementScript::OnDirectionChange, this
				);
			}

			void OnMoveStart() {
				auto active{ entity.Get<AnimationMap>().GetActive() };
				PTGN_ASSERT(active.has_value());
				active->Start(false);
			}

			void OnMoveStop() {
				auto active{ entity.Get<AnimationMap>().GetActive() };
				PTGN_ASSERT(active.has_value());
				active->Reset();
			}

			void OnDirectionChange(MoveDirection) {
				auto& a{ entity.Get<AnimationMap>() };
				auto dir{ entity.Get<TopDownMovement>().GetDirection() };
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
					prev_active->Reset();
				}
				auto current_active{ a.GetActive() };
				PTGN_ASSERT(current_active.has_value());
				current_active->Start(false);
			}
		};

		AddScript<MovementScript>(player);
	}

	return player;
}

} // namespace ptgn