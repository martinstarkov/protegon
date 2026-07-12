#include "runtime/ecs/manager.h"

#include <ecs/ecs.h>

namespace ptgn {

void Manager::ClearEntities() {
	for (auto entity : Entities()) {
		entity.Destroy();
	}
}

Manager::Manager(ManagerBase&& manager) : ManagerBase{ std::move(manager) } {}

} // namespace ptgn