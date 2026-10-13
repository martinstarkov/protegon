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
	timer_.Reset();
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

void Lifetime::Advance(Entity entity, millisecondsf amount) {
	if (amount <= millisecondsf{ 0.0f }) {
		return;
	}

	if (!timer_.IsRunning() && !timer_.IsPaused()) {
		millisecondsf elapsed{ timer_.ElapsedDuration<millisecondsf>() };
		timer_.Start();
		timer_.AddElapsed(elapsed);
	}

	timer_.AddElapsed(amount);
	timer_.Pause();
	ExpireIfComplete(entity);
}

void Lifetime::Rewind(millisecondsf amount) {
	if (amount <= millisecondsf{ 0.0f }) {
		return;
	}

	timer_.RemoveElapsed(amount);
	if (Elapsed() <= millisecondsf{ 0.0f }) {
		timer_.Reset();
	} else if (timer_.IsRunning()) {
		timer_.Pause();
	}
}

void Lifetime::SetDuration(Entity entity, millisecondsf value) {
	duration = duration_cast<milliseconds>(millisecondsf{ std::max(0.0f, value.count()) });
	ExpireIfComplete(entity);
}

void Lifetime::AddDuration(Entity entity, millisecondsf amount) {
	if (amount > millisecondsf{ 0.0f }) {
		SetDuration(entity, duration_cast<millisecondsf>(duration) + amount);
	}
}

void Lifetime::RemoveDuration(Entity entity, millisecondsf amount) {
	if (amount > millisecondsf{ 0.0f }) {
		SetDuration(entity, duration_cast<millisecondsf>(duration) - amount);
	}
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

void Lifetime::ExpireIfComplete(Entity entity) {
	if (entity && timer_.HasRun() && timer_.Completed(duration)) {
		entity.Destroy();
	}
}

void Lifetime::Update(Entity entity, secondsf dt) {
	if (!timer_.IsRunning()) {
		return;
	}

	timer_.Update(dt);
	ExpireIfComplete(entity);
}

void Lifetime::Update(Scene& scene, secondsf dt) {
	for (auto [entity, lifetime] : scene.EntitiesWith<Lifetime>()) {
		lifetime.Update(entity, dt);
	}
}

} // namespace ptgn
