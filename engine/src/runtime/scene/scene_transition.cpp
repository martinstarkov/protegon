#include "runtime/scene/scene_transition.h"

#include <chrono>

#include "core/assert.h"
#include "core/math/easing.h"
#include "core/time/time.h"

namespace ptgn {

SceneTransition::SceneTransition(milliseconds duration, milliseconds delay, Ease ease) :
	duration_{ duration }, delay_duration_{ delay }, started_{ delay == 0ms }, ease_{ ease } {
	PTGN_ASSERT(delay >= 0ms);
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
	return ApplyEase(GetUneasedElapsedFraction(), ease_);
}

Ease SceneTransition::GetEase() const {
	return ease_;
}

bool SceneTransition::IsStarted() const {
	return started_;
}

float SceneTransition::GetUneasedElapsedFraction() const {
	PTGN_ASSERT(duration_ >= 0ms);
	if (duration_.count() == 0) {
		return 1.0f;
	}
	return static_cast<float>(elapsed_.count()) / static_cast<float>(duration_.count());
}

milliseconds SceneTransition::GetDuration() const {
	return duration_;
}

} // namespace ptgn