#include "runtime/scene/scene_transition.h"

#include <chrono>

#include "core/assert.h"
#include "core/time/time.h"

namespace ptgn {

SceneTransition::SceneTransition(milliseconds duration, milliseconds delay) :
	duration_{ duration }, delay_duration_{ delay }, started_{ delay == milliseconds{ 0 } } {
	PTGN_ASSERT(delay >= milliseconds{ 0 });
}

void SceneTransition::UpdateTime(secondsf dt) {
	elapsed_ += duration_cast<milliseconds>(dt);
}

void SceneTransition::UpdateDelayTime(secondsf dt) {
	delay_elapsed_ += duration_cast<milliseconds>(dt);
}

bool SceneTransition::IsFinished() const {
	return elapsed_ >= duration_;
}

bool SceneTransition::IsInDelay() const {
	return !started_ && delay_elapsed_ < delay_duration_;
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