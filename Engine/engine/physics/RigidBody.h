#pragma once

#include <limits>

#include "utils/Vector2.h"

constexpr double IMMOVABLE = 0.0; // mass
constexpr double MASSLESS = 0.0; // massless
constexpr double ELASTIC = 1.0; // perfectly elastic collision restitution
constexpr double INFINITE_MASS = std::numeric_limits<double>::infinity();

#define GRAVITY V2_double{ 0, 0 } // pixels per frame
#define DRAGLESS V2_double{ 0, 0 } // drag
#define UNIVERSAL_DRAG DRAGLESS + V2_double{ 0.15, 0.15 }

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