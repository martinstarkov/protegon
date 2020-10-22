#pragma once

#include "Component.h"

#include <map>
#include <string>

#include <engine/utils/Direction.h>

#include <engine/utils/Vector2.h>

struct SpriteInformation {
	V2_double start;
	std::size_t count;
	SpriteInformation(V2_double start = { 0.0, 0.0 }, std::size_t count = 1) : start{ start }, count{ count } {}
};

// json serialization
inline void to_json(nlohmann::json& j, const SpriteInformation& o) {
	j["start"] = o.start;
	j["count"] = o.count;
}

inline void from_json(const nlohmann::json& j, SpriteInformation& o) {
	o = SpriteInformation();
	if (j.find("start") != j.end()) {
		o.start = j.at("start").get<V2_double>();
	}
	if (j.find("count") != j.end()) {
		j.at("count").get<std::size_t>();
	}
}

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

inline void to_json(nlohmann::json& j, const SpriteSheetComponent& o) {
	//j["animations"] = o.animations;
}

inline void from_json(const nlohmann::json& j, SpriteSheetComponent& o) {
	/*if (j.find("animations") != j.end()) {
		o.animations = j.at("animations").get<AnimationMap>();
	}*/
}