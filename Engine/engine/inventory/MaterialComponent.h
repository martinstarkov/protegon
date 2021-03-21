#pragma once

#include "inventory/Material.h"

struct MaterialComponent {
	MaterialComponent(Material type) : type{ type } {}
	Material type;
};