#pragma once

#include "physics/RigidBody.h"

namespace ptgn {

struct RigidBodyComponent {
	RigidBodyComponent() = default;
	~RigidBodyComponent() = default;
	RigidBodyComponent(const RigidBody& body) : body{ body } {}
	RigidBody body;
};

} // namespace ptgn