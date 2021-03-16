#pragma once

#include "math/Vector2.h"

struct SizeComponent {
	SizeComponent() = default;
	SizeComponent(const V2_double& size) : size{ size } {}
	V2_double size;
};