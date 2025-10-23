#include "core/ecs/components/player_controller.h"

#include "audio/audio.h"
#include "core/app/game.h"
#include "core/app/manager.h"
#include "core/ecs/components/animation.h"
#include "core/ecs/components/draw.h"
#include "core/ecs/components/movement.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/utils/move_direction.h"
#include "math/geometry/rect.h"
#include "math/vector2.h"
#include "physics/collider.h"
#include "physics/rigid_body.h"

namespace ptgn {

Entity CreateTopDownPlayer(
	Manager& manager, const V2_float& position, const TopDownPlayerConfig& config
) {
	PTGN_ASSERT(
		game.texture.Has(config.animation_texture_key),
		"Cannot create player with animation key which has not been loaded"
	);

	bool has_sound_key{ game.sound.Has(config.walk_sound_key) };

	auto player{ manager.CreateEntity() };

	SetPosition(player, position);
	player.Add<RigidBody>();
	SetDepth(player, config.depth);

	auto body_hitbox{ manager.CreateEntity() };
	body_hitbox.Add<Collider>(Rect{ config.body_hitbox_size });
	SetPosition(body_hitbox, config.body_hitbox_offset);
	body_hitbox.Add<RigidBody>();

	auto interaction_hitbox{ manager.CreateEntity() };
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
				game.sound.Play(walk_sound_key);
			}
		}
	};

	if (has_sound_key) {
		AddScript<AnimationRepeat>(a0, config.walk_sound_frequency, config.walk_sound_key);
		AddScript<AnimationRepeat>(a1, config.walk_sound_frequency, config.walk_sound_key);
		AddScript<AnimationRepeat>(a2, config.walk_sound_frequency, config.walk_sound_key);
	}

	struct MovementScript : public Script<MovementScript, PlayerMoveScript> {
		void OnMoveStart() override {
			entity.Get<AnimationMap>().GetActive().Start(false);
		}

		void OnMoveStop() override {
			entity.Get<AnimationMap>().GetActive().Reset();
		}

		void OnDirectionChange(MoveDirection) override {
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

	AddScript<MovementScript>(player);

	return player;
}

} // namespace ptgn