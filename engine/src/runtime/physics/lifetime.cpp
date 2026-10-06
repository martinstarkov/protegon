#include "runtime/physics/lifetime.h"

#include <algorithm>

#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

Lifetime::Lifetime(milliseconds lifetime, bool start) : duration{ lifetime } {
	if (start) {
		timer_.Start();
	}
}

void Lifetime::Start() {
	timer_.Restart();
}

void Lifetime::Stop() {
	timer_.Stop();
}

void Lifetime::Reset() {
	timer_.Reset();
}

void Lifetime::Pause() {
	timer_.Pause();
}

void Lifetime::Resume() {
	timer_.Resume();
}

void Lifetime::Advance(millisecondsf amount) {
	timer_.AddElapsed(amount);
}

void Lifetime::Rewind(millisecondsf amount) {
	timer_.RemoveElapsed(amount);
}

millisecondsf Lifetime::Elapsed() const {
	return timer_.ElapsedDuration<millisecondsf>();
}

millisecondsf Lifetime::Remaining() const {
	millisecondsf lifetime{ duration_cast<millisecondsf>(duration) };
	return millisecondsf{ std::max(0.0f, lifetime.count() - Elapsed().count()) };
}

float Lifetime::Progress() const {
	return timer_.ElapsedFraction(duration);
}

bool Lifetime::IsRunning() const {
	return timer_.IsRunning();
}

bool Lifetime::IsPaused() const {
	return timer_.IsPaused();
}

bool Lifetime::HasRun() const {
	return timer_.HasRun();
}

void Lifetime::Update(Entity entity, secondsf dt) {
	timer_.Update(dt);
	if (timer_.Completed(duration)) {
		entity.Destroy();
	}
}

void Lifetime::Update(Scene& scene, secondsf dt) {
	for (auto [entity, lifetime] : scene.EntitiesWith<Lifetime>()) {
		lifetime.Update(entity, dt);
	}
}

} // namespace ptgn
