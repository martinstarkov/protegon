#include "runtime/physics/lifetime.h"

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
	timer_.Start();
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