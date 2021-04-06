#pragma once

#include "ecs/System.h"

// TODO: Take StateComponent (somehow) and set current SpriteComponent sprite equal to the corresponding spritesheet image

class AnimationSystem : public ecs::System<AnimationComponent, SpriteComponent, DirectionComponent> {
public:
	virtual void Update() override final {
		for (auto [entity, animation_component, sprite, dir] : entities) {
			//LOG("Direction: " << (int)direction->direction);
			auto animation = sprite.sprite_map.GetAnimation(animation_component.current_animation.c_str());
			sprite.current_sprite.position.y = animation.position.y;
			sprite.current_sprite.size = animation.sprite_size;
			if (animation_component.current_animation != "" && animation_component.counter == -1 || dir.x_direction != dir.x_previous_direction || dir.y_direction != dir.y_previous_direction) {
				animation_component.counter = 0;
			}
			unsigned int total_timer = animation_component.counter % (animation_component.cycles_per_frame * animation.sprite_count);
			if (total_timer % animation_component.cycles_per_frame == 0) {
				animation_component.frame = static_cast<int>(total_timer / animation_component.cycles_per_frame);
				sprite.current_sprite.position.x = animation.position.x + (sprite.current_sprite.size.x + animation.spacing) * animation_component.frame;
			}
			if (total_timer == 0) { // reset counter so it doesn't grow infinitely
				animation_component.counter = 0;
			}
			animation_component.counter++;
		}
	}
};