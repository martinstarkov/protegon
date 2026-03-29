#include "runtime/physics/lifetime.h"

#include "core/time/time.h"
#include "core/time/timer.h"
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

void Lifetime::Update(Entity entity) const {
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