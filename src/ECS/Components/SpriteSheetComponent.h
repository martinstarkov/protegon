#pragma once

#include "Component.h"

#include <map>
#include <string>

#include "SDL.h"
#include "../../AABB.h"

// Turn sprite component into sprite sheet that feeds into AnimationSystem and create custom systems for different states (MovementStateSystem, etc)

struct SpriteInformation {
    SDL_Rect start;
    unsigned int count;
};

struct SpriteSheetComponent : public Component<SpriteSheetComponent> {
	const char* path;
	using AnimationID = std::string;
    std::map<AnimationID, SpriteInformation> animations;
	// TODO: Make a spritesheet component which includes multiple animations
	SpriteSheetComponent(const char* path = nullptr) : path(path) {
        // read path file and emplace into animations as below
        // TEMPORARY: Only supports the player_anim.png
		// place SpriteInformation struct into _animations map
		animations.emplace(std::make_pair("Walk", SpriteInformation{ AABB(0, 0, 16, 16).AABBtoRect(), 8 }));
		animations.emplace(std::make_pair("Run", SpriteInformation{ AABB(0, 16, 16, 16).AABBtoRect(), 8 }));
	}
	virtual ~SpriteSheetComponent() override {}
};