#pragma once

#include "Component.h"

#include "../../Vec2D.h"

struct MotionComponent : public Component<MotionComponent> {
	Vec2D velocity;
	Vec2D acceleration;
	Vec2D terminalVelocity;
	MotionComponent(Vec2D velocity = Vec2D(), Vec2D acceleration = Vec2D(), Vec2D terminalVelocity = Vec2D().infinite()) : velocity(velocity), acceleration(acceleration), terminalVelocity(terminalVelocity) {}
	virtual void init() override final;
};

// json serialization
inline void to_json(nlohmann::json& j, const MotionComponent& o) {
	j["velocity"] = o.velocity;
	j["acceleration"] = o.acceleration;
	j["terminalVelocity"] = o.terminalVelocity;
}

inline void from_json(const nlohmann::json& j, MotionComponent& o) {
	o = MotionComponent(
		j.at("velocity").get<Vec2D>(),
		j.at("acceleration").get<Vec2D>(),
		j.at("terminalVelocity").get<Vec2D>() // might not work due to numeric limits
	);
}