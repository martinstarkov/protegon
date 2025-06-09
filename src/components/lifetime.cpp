#include "components/lifetime.h"

#include "core/entity.h"
#include "core/manager.h"
#include "core/time.h"
#include "core/timer.h"

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

void Lifetime::Update(Manager& manager) {
	for (auto [entity, lifetime] : manager.EntitiesWith<Lifetime>()) {
		lifetime.Update(entity);
	}

	manager.Refresh();
}

} // namespace ptgn