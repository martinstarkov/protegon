#include "player/player_controller.h"

#include "audio/audio.h"
#include "common/move_direction.h"
#include "components/animation.h"
#include "components/common.h"
#include "components/movement.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "physics/collision/collider.h"
#include "physics/rigid_body.h"
#include "renderer/api/origin.h"
#include "scene/scene.h"

namespace ptgn {

Entity CreateTopDownPlayer(
	Scene& scene, const V2_float& position, const TopDownPlayerConfig& config
) {
	PTGN_ASSERT(
		game.texture.Has(config.animation_texture_key),
		"Cannot create player with animation key which has not been loaded"
	);
	PTGN_ASSERT(
		game.sound.Has(config.walk_sound_key),
		"Cannot create player with walk sound key which has not been loaded"
	);

	auto player{ scene.CreateEntity() };

	player.SetPosition(position);
	player.Add<RigidBody>();
	player.Enable();
	player.SetDepth(config.depth);

	auto body_hitbox{ scene.CreateEntity() };
	body_hitbox.Add<Collider>(Rect{ config.body_hitbox_size });
	body_hitbox.SetPosition(config.body_hitbox_offset);
	body_hitbox.Enable();
	body_hitbox.Add<RigidBody>();

	auto interaction_hitbox{ scene.CreateEntity() };
	auto& interaction_collider =
		interaction_hitbox.Add<Collider>(Rect{ config.interaction_hitbox_size });
	interaction_collider.overlap_only = true;
	interaction_hitbox.SetPosition({});
	interaction_hitbox.Enable();

	player.AddChild(body_hitbox, "body");
	player.AddChild(interaction_hitbox, "interaction");

	auto& movement{ player.Add<TopDownMovement>() };
	movement.max_speed		  = config.max_speed;
	movement.max_acceleration = config.max_acceleration;
	movement.max_deceleration = config.max_deceleration;
	movement.max_turn_speed	  = config.max_turn_speed;
	movement.friction		  = config.friction;

	auto& anim_map = player.Add<AnimationMap>(
		"down", CreateAnimation(
					scene, config.animation_texture_key, config.animation_frame_count.x,
					config.animation_duration, config.animation_frame_size
				)
	);

	auto& a0 = anim_map.GetActive();
	auto& a1 = anim_map.Load(
		"right", CreateAnimation(
					 scene, config.animation_texture_key, config.animation_frame_count.x,
					 config.animation_duration, config.animation_frame_size, -1,
					 V2_float{ 0, config.animation_frame_size.y }
				 )
	);
	auto& a2 = anim_map.Load(
		"up", CreateAnimation(
				  scene, config.animation_texture_key, config.animation_frame_count.x,
				  config.animation_duration, config.animation_frame_size, -1,
				  V2_float{ 0, 2.0f * config.animation_frame_size.y }
			  )
	);

	a0.SetParent(player);
	a1.SetParent(player);
	a2.SetParent(player);

	struct AnimationRepeat : public Script<AnimationRepeat> {
		AnimationRepeat() = default;

		AnimationRepeat(std::size_t walk_frequency, std::string_view walk_sound) :
			walk_sound_frequency{ walk_frequency }, walk_sound_key{ walk_sound } {}

		std::size_t walk_sound_frequency{ 0 };
		std::string_view walk_sound_key;

		void OnAnimationFrameChange(std::size_t frame) override {
			if (frame % walk_sound_frequency == 0) {
				game.sound.Play(walk_sound_key);
			}
		}
	};

	a0.AddScript<AnimationRepeat>(config.walk_sound_frequency, config.walk_sound_key);
	a1.AddScript<AnimationRepeat>(config.walk_sound_frequency, config.walk_sound_key);
	a2.AddScript<AnimationRepeat>(config.walk_sound_frequency, config.walk_sound_key);

	struct MovementScript : public Script<MovementScript> {
		void OnMoveStart() override {
			entity.Get<AnimationMap>().GetActive().Start(false);
		}

		void OnMoveStop() override {
			entity.Get<AnimationMap>().GetActive().Reset();
		}

		void OnMoveDirectionChange(MoveDirection) override {
			auto& a{ entity.Get<AnimationMap>() };
			auto dir{ entity.Get<TopDownMovement>().GetDirection() };
			auto& prev_active{ a.GetActive() };
			bool active_changed{ false };

			switch (dir) {
				case MoveDirection::Down:	   active_changed = a.SetActive("down"); break;
				case MoveDirection::Up:		   active_changed = a.SetActive("up"); break;
				case MoveDirection::Left:	   [[fallthrough]];
				case MoveDirection::DownLeft:  [[fallthrough]];
				case MoveDirection::UpLeft:	   [[fallthrough]];
				case MoveDirection::UpRight:   [[fallthrough]];
				case MoveDirection::DownRight: [[fallthrough]];
				case MoveDirection::Right:	   active_changed = a.SetActive("right"); break;
				default:					   break;
			}
			if (active_changed) {
				prev_active.Reset();
			}
			auto& current_active{ a.GetActive() };
			current_active.Start(false);
		}
	};

	player.AddScript<MovementScript>();

	return player;
}

} // namespace ptgn