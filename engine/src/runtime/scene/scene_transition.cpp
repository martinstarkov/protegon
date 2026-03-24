#include "runtime/scene/scene_transition.h"

#include <chrono>

#include "core/assert.h"
#include "core/time/time.h"

namespace ptgn {

SceneTransition::SceneTransition(milliseconds duration) : duration_{ duration } {}

void SceneTransition::UpdateTime(secondsf dt) {
	elapsed_ += duration_cast<milliseconds>(dt);
}

bool SceneTransition::IsFinished() const {
	return elapsed_ >= duration_;
}

float SceneTransition::GetElapsedFraction() const {
	PTGN_ASSERT(duration_ >= milliseconds{ 0 });
	if (duration_.count() == 0) {
		return 1.0f;
	}
	return static_cast<float>(elapsed_.count()) / static_cast<float>(duration_.count());
}

milliseconds SceneTransition::GetDuration() const {
	return duration_;
}

} // namespace ptgn