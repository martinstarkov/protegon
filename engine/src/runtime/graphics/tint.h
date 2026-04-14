#pragma once

#include "core/graphics/color.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

namespace impl {

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

} // namespace impl

/// @param color color::White will clear any tint.
void SetTint(Entity entity, Color color = color::White);

Color GetTint(Entity entity);

} // namespace ptgn