#include "ecs/components/lifetime.h"

#include "ecs/entity.h"
#include "core/util/time.h"
#include "core/util/timer.h"
#include "world/scene/scene.h"

namespace ptgn {

Lifetime::Lifetime(milliseconds lifetime, bool start) : duration{ lifetime } {
	if (start) {
		timer_.Start();
	}
}

// Will restart if lifetime is already running.
void Lifetime::Start() {
	timer_.Start();
}

void Lifetime::Update(Entity& entity) const {
	if (timer_.Completed(duration)) {
		entity.Destroy();
	}
}

void Lifetime::Update(Scene& scene) {
	for (auto [entity, lifetime] : scene.EntitiesWith<Lifetime>()) {
		lifetime.Update(entity);
	}

	scene.Refresh();
}

} // namespace ptgn