#pragma once

#include "ecs/components/generic.h"

namespace ptgn {

struct SceneKey : public HashComponent {
	using HashComponent::HashComponent;
};

} // namespace ptgn