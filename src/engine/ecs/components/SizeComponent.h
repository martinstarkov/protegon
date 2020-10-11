#pragma once

#include <engine/utils/Vector2.h>

struct SizeComponent {
	SizeComponent(V2_int size) : size{ size } {}
	V2_int size;
};