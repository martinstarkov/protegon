#pragma once

#include "Component.h"

#include <map>
#include <string>

#include "utils/Direction.h"
#include "utils/Vector2.h"

struct SpriteInformation {
	V2_double start;
	std::size_t count;
	SpriteInformation(V2_double start = { 0.0, 0.0 }, std::size_t count = 1) : start{ start }, count{ count } {}
};

struct SpriteSheetComponent {
	std::map<std::string, std::map<Direction, SpriteInformation>> animations;
	SpriteSheetComponent() {
        // read path file and emplace into animations as below
        // TEMPORARY: Only supports the player_anim.png
		// place SpriteInformation struct into _animations maps
		animations.insert({ "idle", {
			{ Direction::DOWN, SpriteInformation{ V2_double{ 0, 0 }, 5 } }
		} });
		animations.insert({ "walk", {
			{ Direction::UP, SpriteInformation{ V2_double{ 0, 1 }, 9 } },
			{ Direction::RIGHT, SpriteInformation{ V2_double{ 0, 2 }, 9 } },
			{ Direction::DOWN, SpriteInformation{ V2_double{ 0, 3 }, 9 } }
		} });
	}
	SpriteInformation GetSpriteInformation(const std::string& name, Direction direction) {
		auto animation_it = animations.find(name); // animation iterator
		assert(animation_it != animations.end() && "Animation name not found in animation component");
		if (direction == Direction::LEFT) { // ignore SDL flipped direction
			direction = Direction::RIGHT;
		}
		assert(animation_it->second.size() > 0 && "Cannot fetch sprite information for size 0 sprite sheet map");
		auto direction_it = animation_it->second.find(direction); // direction iterator
		if (direction_it != animation_it->second.end()) {
			return direction_it->second; // SpriteInformation
		} else {
			// return the first DirectionMap entry if specified direction is not found
			// CONSIDER: Somehow improve this in the future?
			return animation_it->second.begin()->second;
		}
	}
	~SpriteSheetComponent() {}
};