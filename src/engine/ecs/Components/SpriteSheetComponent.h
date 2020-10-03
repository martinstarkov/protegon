#pragma once

#include "Component.h"

#include <map>
#include <string>

#include <Direction.h>
#include <Vec2D.h>

struct SpriteInformation {
	Vec2D start;
	std::size_t count;
	SpriteInformation(Vec2D start = Vec2D(), std::size_t count = 1) : start(start), count(count) {}
};

// json serialization
inline void to_json(nlohmann::json& j, const SpriteInformation& o) {
	j["start"] = o.start;
	j["count"] = o.count;
}

inline void from_json(const nlohmann::json& j, SpriteInformation& o) {
	o = SpriteInformation();
	if (j.find("start") != j.end()) {
		o.start = j.at("start").get<Vec2D>();
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
			{ Direction::DOWN, SpriteInformation{ Vec2D(0, 0), 5 } }
		} });
		animations.insert({ "walk", {
			{ Direction::UP, SpriteInformation{ Vec2D(0, 1), 9 } },
			{ Direction::RIGHT, SpriteInformation{ Vec2D(0, 2), 9 } },
			{ Direction::DOWN, SpriteInformation{ Vec2D(0, 3), 9 } }
		} });
	}
	SpriteInformation getSpriteInformation(const std::string& name, Direction direction) {
		auto aIt = animations.find(name); // animation iterator
		assert(aIt != animations.end() && "AnimationName not found in AnimationComponent");
		if (direction == Direction::LEFT) { // ignore SDL flipped direction
			direction = Direction::RIGHT;
		}
		assert(aIt->second.size() > 0 && "Cannot fetch spriteInformation for size 0 spriteSheetMap");
		auto dIt = aIt->second.find(direction); // direction iterator
		if (dIt != aIt->second.end()) {
			return dIt->second; // SpriteInformation
		} else {
			// return the first DirectionMap entry if specified direction is not found
			// CONSIDER: Somehow improve this in the future?
			return aIt->second.begin()->second;
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