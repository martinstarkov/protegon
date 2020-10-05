#pragma once

#include <limits>

#include <engine/utils/Vector2.h>

constexpr double IMMOVABLE = 0.0; // mass
constexpr double MASSLESS = 0.0; // massless
constexpr double ELASTIC = 1.0; // perfectly elastic collision restitution
constexpr double INFINITE_MASS = std::numeric_limits<double>::infinity();

#define UNIVERSAL_DRAG DRAGLESS + 0.15
#define GRAVITY V2_double{ 0, 0 } // pixels per frame
#define DRAGLESS V2_double{ 0, 0 } // drag

struct RigidBody {
	V2_double velocity;
	V2_double terminal_velocity;
	V2_double acceleration;
	V2_double maximum_acceleration;
	V2_double drag;
	V2_double gravity;
	double mass;
	double inverse_mass;
	double restitution;
	RigidBody(V2_double drag = DRAGLESS, V2_double gravity = GRAVITY, double restitution = ELASTIC, double mass = IMMOVABLE, V2_double maximum_acceleration = { std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() }) : drag{ drag }, gravity{ gravity }, restitution{ restitution }, mass{ mass }, maximum_acceleration{ maximum_acceleration }, terminal_velocity{ std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity() } {
		Init();
	}
	void Init();
	void ComputeTerminalVelocity();
};

// json serialization
inline void to_json(nlohmann::json& j, const RigidBody& o) {
	j["velocity"] = o.velocity;
	j["terminal_velocity"] = o.terminal_velocity;
	j["acceleration"] = o.acceleration;
	j["maximum_acceleration"] = o.maximum_acceleration;
	j["drag"] = o.drag;
	j["gravity"] = o.gravity;
	j["mass"] = o.mass;
	j["restitution"] = o.restitution;
}

inline void from_json(const nlohmann::json& j, RigidBody& o) {
	if (j.find("velocity") != j.end()) {
		o.velocity = j.at("velocity").get<V2_double>();
	}
	if (j.find("terminal_velocity") != j.end()) {
		o.terminal_velocity = j.at("terminal_velocity").get<V2_double>();
	}
	if (j.find("acceleration") != j.end()) {
		o.acceleration = j.at("acceleration").get<V2_double>();
	}
	if (j.find("maximum_acceleration") != j.end()) {
		o.maximum_acceleration = j.at("maximum_acceleration").get<V2_double>();
	}
	if (j.find("drag") != j.end()) {
		o.drag = j.at("drag").get<V2_double>();
	}
	if (j.find("gravity") != j.end()) {
		o.gravity = j.at("gravity").get<V2_double>();
	}
	if (j.find("mass") != j.end()) {
		o.mass = j.at("mass").get<double>();
	}
	if (j.find("restitution") != j.end()) {
		o.restitution = j.at("restitution").get<double>();
	}
	o.Init();
}