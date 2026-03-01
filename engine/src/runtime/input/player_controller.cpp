#include "runtime/input/player_controller.h"

#include <optional>

#include "app/context.h"
#include "core/assert.h"
#include "core/event/dispatcher.h"
#include "core/math/geometry/rect.h"
#include "core/math/vector2.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/ecs/components/draw.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/input/move_direction.h"
#include "runtime/input/movement.h"
#include "runtime/physics/collider.h"
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

	/*
	// TODO: Fix this.

	if (config.animation_texture_key.has_value()) {
		PTGN_ASSERT(
			scene.app().asset.HasTexture(*config.animation_texture_key),
			"Cannot create player with animation key which has not been loaded"
		);

		auto& anim_map = player.Add<AnimationMap>(
		"down", CreateAnimation(
					manager, config.animation_texture_key, {}, config.animation_frame_count.x,
					config.animation_duration, config.animation_frame_size
				)
	);

	auto& a0 = anim_map.GetActive();
	auto& a1 = anim_map.Load(
		"right", CreateAnimation(
					 manager, config.animation_texture_key, {}, config.animation_frame_count.x,
					 config.animation_duration, config.animation_frame_size, -1,
					 V2_float{ 0, config.animation_frame_size.y }
				 )
	);
	auto& a2 = anim_map.Load(
		"up", CreateAnimation(
				  manager, config.animation_texture_key, {}, config.animation_frame_count.x,
				  config.animation_duration, config.animation_frame_size, -1,
				  V2_float{ 0, 2.0f * config.animation_frame_size.y }
			  )
	);

	SetParent(a0, player);
	SetParent(a1, player);
	SetParent(a2, player);

	struct AnimationRepeat : public Script<AnimationRepeat, AnimationScript> {
		AnimationRepeat() = default;

		AnimationRepeat(std::size_t walk_frequency, std::string_view walk_sound) :
			walk_sound_frequency{ walk_frequency }, walk_sound_key{ walk_sound } {}

		std::size_t walk_sound_frequency{ 0 };
		std::string_view walk_sound_key;

		void OnAnimationFrameChange() override {
			auto frame{ Animation{ entity }.GetCurrentFrame() };
			if (frame % walk_sound_frequency == 0) {
				// TODO: Fix this.
				// Application::Get().sound.Play(walk_sound_key);
			}
		}
	};

	struct MovementScript : public Script {
		void OnEvent(EventDispatcher d) override {
			d.Dispatch<PlayerMoveStarted>([&](auto) { OnMoveStart(); });
			d.Dispatch<PlayerMoveStopped>([&](auto) { OnMoveStop(); });
			d.Dispatch<PlayerMoveDirectionChanged>([&](const auto& e) {
				OnDirectionChange(e.current_direction);
			});
		}

		void OnMoveStart() {
			// TODO: Fix this.
			entity.Get<AnimationMap>().GetActive().Start(false);
		}

		void OnMoveStop() {
			// TODO: Fix this.
			entity.Get<AnimationMap>().GetActive().Reset();
		}

		void OnDirectionChange(MoveDirection) {
			// TODO: Fix this.
			auto& a{ entity.Get<AnimationMap>() };
			auto dir{ entity.Get<TopDownMovement>().GetDirection() };
			auto& prev_active{ a.GetActive() };
			bool active_changed{ false };

			switch (dir) {
				using enum ptgn::MoveDirection;
				case Down:	   active_changed = a.SetActive("down"); break;
				case Up:		   active_changed = a.SetActive("up"); break;
				case Left:	   [[fallthrough]];
				case DownLeft:  [[fallthrough]];
				case UpLeft:	   [[fallthrough]];
				case UpRight:   [[fallthrough]];
				case DownRight: [[fallthrough]];
				case Right:	   active_changed = a.SetActive("right"); break;
				default:					   break;
			}
			if (active_changed) {
				prev_active.Reset();
			}
			auto& current_active{ a.GetActive() };
			current_active.Start(false);
		}
	};

	AddScript<MovementScript>(player);

	}

	*/

	// TODO: Fix this.
	// if (config.walk_sound_key.has_value()) {
	//	PTGN_ASSERT(scene.app().asset.HasSound(*config.walk_sound_key));
	//	AddScript<AnimationRepeat>(a0, config.walk_sound_frequency, config.walk_sound_key);
	//	AddScript<AnimationRepeat>(a1, config.walk_sound_frequency, config.walk_sound_key);
	//	AddScript<AnimationRepeat>(a2, config.walk_sound_frequency, config.walk_sound_key);
	//}

	return player;
}

} // namespace ptgn