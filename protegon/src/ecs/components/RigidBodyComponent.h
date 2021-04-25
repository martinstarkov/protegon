#pragma once

#include "physics/RigidBody.h"

namespace engine {

struct RigidBodyComponent {
	RigidBodyComponent() = default;
	RigidBodyComponent(const RigidBody& body) : body{ body } {}
	RigidBody body;
};

} // namespace engine