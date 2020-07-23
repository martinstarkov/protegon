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

using DirectionMap = std::map<Direction, SpriteInformation>;
using AnimationMap = std::map<AnimationName, DirectionMap>;

struct SpriteSheetComponent : public Component<SpriteSheetComponent> {
	std::string path;
    AnimationMap animations;
	// TODO: Make a spritesheet component which includes multiple animations
	SpriteSheetComponent(AnimationMap&& animations) : animations(animations) {}
	SpriteSheetComponent(std::string path = "") : path(path) {
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
	SpriteInformation getSpriteInformation(AnimationName name, Direction direction);
	virtual ~SpriteSheetComponent() override {}
};

// json serialization

inline void to_json(nlohmann::json& j, const SpriteInformation& o) {
	j["start"] = o.start;
	j["count"] = o.count;
}

inline void from_json(const nlohmann::json& j, SpriteInformation& o) {
	o = SpriteInformation{
		j.at("start").get<Vec2D>(),
		j.at("count").get<std::size_t>(),
	};
}

inline void to_json(nlohmann::json& j, const SpriteSheetComponent& o) {
	//json map;
	//for (auto& pair : o.animations) {
	//	map[pair.first] = pair.second;
	//}
	//j["animations"] = map;
	j["animations"] = o.animations;
}

inline void from_json(const nlohmann::json& j, SpriteSheetComponent& o) {
	o = SpriteSheetComponent(
		static_cast<AnimationMap>(j.at("animations").get<AnimationMap>()) // might fail
	);
}