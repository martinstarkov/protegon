#pragma once

#include <limits>

#include "common.h"
#include "Vec2D.h"

constexpr double IMMOVABLE = 0.0; // mass
constexpr double GRAVITY = 0.0; // pixels per frame
constexpr double MASSLESS = 0.0; // massless
constexpr double ELASTIC = 1.0; // perfectly elastic collision restitution
constexpr double DRAGLESS = 0.0; // drag

struct RigidBody {
	Vec2D velocity;
	Vec2D terminalVelocity;
	Vec2D acceleration;
	Vec2D maximumAcceleration;
	Vec2D drag;
	Vec2D gravity;
	double mass;
	double inverseMass;
	double restitution;
	RigidBody(Vec2D drag = Vec2D(DRAGLESS), Vec2D gravity = Vec2D(GRAVITY), double restitution = ELASTIC, double mass = IMMOVABLE, Vec2D maximumAcceleration = Vec2D(INFINITE)) : drag(drag), gravity(gravity), restitution(restitution), mass(mass), maximumAcceleration(maximumAcceleration), terminalVelocity(Vec2D(INFINITE)) {
		init();
	}
	void init();
	void computeTerminalVelocity();
};

// json serialization
inline void to_json(nlohmann::json& j, const RigidBody& o) {
	j["velocity"] = o.velocity;
	j["terminalVelocity"] = o.terminalVelocity;
	j["acceleration"] = o.acceleration;
	j["maximumAcceleration"] = o.maximumAcceleration;
	j["drag"] = o.drag;
	j["gravity"] = o.gravity;
	j["mass"] = o.mass;
	j["restitution"] = o.restitution;
}

inline void from_json(const nlohmann::json& j, RigidBody& o) {
	if (j.find("velocity") != j.end()) {
		o.velocity = j.at("velocity").get<Vec2D>();
	}
	if (j.find("terminalVelocity") != j.end()) {
		o.terminalVelocity = j.at("terminalVelocity").get<Vec2D>();
	}
	if (j.find("acceleration") != j.end()) {
		o.acceleration = j.at("acceleration").get<Vec2D>();
	}
	if (j.find("maximumAcceleration") != j.end()) {
		o.maximumAcceleration = j.at("maximumAcceleration").get<Vec2D>();
	}
	if (j.find("drag") != j.end()) {
		o.drag = j.at("drag").get<Vec2D>();
	}
	if (j.find("gravity") != j.end()) {
		o.gravity = j.at("gravity").get<Vec2D>();
	}
	if (j.find("mass") != j.end()) {
		o.mass = j.at("mass").get<double>();
	}
	if (j.find("restitution") != j.end()) {
		o.restitution = j.at("restitution").get<double>();
	}
	o.init();
}