#include "runtime/scene/scene_transition.h"

#include <chrono>

#include "core/assert.h"
#include "core/math/easing.h"
#include "core/util/time.h"

namespace ptgn {

SceneTransition::SceneTransition(milliseconds duration, milliseconds delay, Ease ease) :
	duration_{ duration }, delay_duration_{ delay }, started_{ delay == 0ms }, ease_{ ease } {
	PTGN_ASSERT(delay >= 0ms);
}

void SceneTransition::UpdateTime(milliseconds dt) {
	elapsed_ += dt;
}

void SceneTransition::UpdateDelayTime(milliseconds dt) {
	delay_elapsed_ += dt;
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

	if (duration_ == 0ms) {
		return 1.0f;
	}

	return millisecondsf{ elapsed_ } / duration_;
}

milliseconds SceneTransition::GetDuration() const {
	return duration_;
}

} // namespace ptgn