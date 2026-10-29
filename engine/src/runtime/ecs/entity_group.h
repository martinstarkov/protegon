#pragma once

#include "runtime/ecs/group_overlay.h"
#include "serialization/serialize.h"

namespace ptgn {

// A Group is an ordinary, transform-transparent hierarchy parent. Only groups directly
// under a GroupStack are managed by that stack. Other groups are independent organizers.
struct Group {
	OverlayMode overlay{ OverlayMode::Disable };
	bool show_disabled_style{ false };
	bool block_background_input{ false };

	PTGN_REFLECT(Group, overlay, show_disabled_style, block_background_input)
};

} // namespace ptgn
