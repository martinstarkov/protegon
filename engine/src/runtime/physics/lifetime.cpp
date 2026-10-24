#include "runtime/physics/lifetime.h"

#include <algorithm>

#include "core/util/time.h"
#include "core/util/timer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/group_stack.h"
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

void Lifetime::Advance(Entity entity, milliseconds amount) {
	if (amount <= 0ms) {
		return;
	}

	if (!timer_.IsRunning() && !timer_.IsPaused()) {
		milliseconds elapsed{ timer_.ElapsedDuration<milliseconds>() };
		timer_.Start();
		timer_.AddElapsed(elapsed);
	}

	timer_.AddElapsed(amount);
	timer_.Pause();
	ExpireIfComplete(entity);
}

void Lifetime::Rewind(milliseconds amount) {
	if (amount <= 0ms) {
		return;
	}

	timer_.RemoveElapsed(amount);
	if (Elapsed() <= 0ms) {
		timer_.Reset();
	} else if (timer_.IsRunning()) {
		timer_.Pause();
	}
}

void Lifetime::SetDuration(Entity entity, milliseconds value) {
	duration = std::max(0ms, value);
	ExpireIfComplete(entity);
}

void Lifetime::AddDuration(Entity entity, milliseconds amount) {
	if (amount > 0ms) {
		SetDuration(entity, duration + amount);
	}
}

void Lifetime::RemoveDuration(Entity entity, milliseconds amount) {
	if (amount > 0ms) {
		SetDuration(entity, duration - amount);
	}
}

milliseconds Lifetime::Elapsed() const {
	return timer_.ElapsedDuration<milliseconds>();
}

milliseconds Lifetime::Remaining() const {
	return std::max(0ms, duration - Elapsed());
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
		if (!IsGroupStackActive(entity)) {
			continue;
		}
		lifetime.Update(entity, dt);
	}
}

} // namespace ptgn
