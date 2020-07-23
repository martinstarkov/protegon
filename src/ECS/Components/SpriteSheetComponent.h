#pragma once

#include "Component.h"

#include <map>
#include <string>

#include "../../Direction.h"
#include "../../Vec2D.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteInformation {
	Vec2D start;
	std::size_t count;
};

struct SpriteSheetComponent : public Component<SpriteSheetComponent> {
	std::string path;
	using AnimationMap = std::map<Direction, SpriteInformation>;
    std::map<AnimationName, AnimationMap> animations;
	// TODO: Make a spritesheet component which includes multiple animations
	SpriteSheetComponent(std::string path = "") : path(path) {
        // read path file and emplace into animations as below
        // TEMPORARY: Only supports the player_anim.png
		// place SpriteInformation struct into _animations mapS
		AnimationMap idle = {
			{ Direction::DOWN, SpriteInformation{ Vec2D(0, 0), 5 } }
		};
		AnimationMap walk = {
			{ Direction::UP, SpriteInformation{ Vec2D(0, 1), 9 } },
			{ Direction::RIGHT, SpriteInformation{ Vec2D(0, 2), 9 } },
			{ Direction::DOWN, SpriteInformation{ Vec2D(0, 3), 9 } }
		};
		animations.emplace("idle", std::move(idle));
		animations.emplace("walk", std::move(walk));
	}
	SpriteInformation getSpriteInformation(AnimationName name, Direction direction);
	virtual ~SpriteSheetComponent() override {}
};