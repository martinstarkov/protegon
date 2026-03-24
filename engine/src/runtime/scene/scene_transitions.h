#pragma once

#include "core/time/time.h"
#include "runtime/scene/scene_transition.h"

namespace ptgn {

struct TimedTransition : public SceneTransition {
	explicit TimedTransition(milliseconds duration) : SceneTransition{ duration } {}
};

} // namespace ptgn