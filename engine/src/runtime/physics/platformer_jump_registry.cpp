#include "runtime/physics/platformer_jump_registry.h"

#include <algorithm>
#include <memory>
#include <ranges>
#include <utility>
#include <vector>

#include "runtime/physics/movement.h"

namespace ptgn {

namespace {

std::vector<RegisteredPlatformerJumpController>& Storage() {
	static std::vector<RegisteredPlatformerJumpController> controllers;
	return controllers;
}

} // namespace

void PlatformerJumpControllerRegistry::Register(RegisteredPlatformerJumpController controller) {
	if (controller.key.empty() || controller.component_hash == 0 || !controller.has ||
		!controller.get || !controller.add_default || !controller.remove || !controller.update) {
		return;
	}

	auto& controllers{ Storage() };
	const auto it{
		std::ranges::find(controllers, controller.key, &RegisteredPlatformerJumpController::key)
	};

	if (it != controllers.end()) {
		*it = std::move(controller);
		return;
	}

	controllers.emplace_back(std::move(controller));
}

const RegisteredPlatformerJumpController* PlatformerJumpControllerRegistry::Find(
	std::string_view key
) {
	const auto& controllers{ Storage() };
	const auto it{
		std::ranges::find(controllers, key, &RegisteredPlatformerJumpController::key)
	};
	return it == controllers.end() ? nullptr : std::addressof(*it);
}

const std::vector<RegisteredPlatformerJumpController>&
PlatformerJumpControllerRegistry::Controllers() {
	return Storage();
}

void SyncPlatformerJumpController(Entity entity) {
	if (!entity) {
		return;
	}

	if (!entity.Has<PlatformerMovement>()) {
		for (const auto& controller : PlatformerJumpControllerRegistry::Controllers()) {
			if (controller.has(entity)) {
				controller.remove(entity);
			}
		}
		return;
	}

	const auto& selected{ entity.Get<PlatformerMovement>().jump_controller };
	if (selected.empty()) {
		return;
	}

	if (const auto* controller{ PlatformerJumpControllerRegistry::Find(selected) };
		controller && !controller->has(entity)) {
		controller->add_default(entity);
	}
}

void ClearPlatformerJumpController(Entity entity) {
	if (!entity || !entity.Has<PlatformerMovement>()) {
		return;
	}

	entity.Get<PlatformerMovement>().jump_controller.clear();
	SyncPlatformerJumpController(entity);
}

void SetPlatformerJumpController(Entity entity, std::string_view key) {
	if (!entity || !entity.Has<PlatformerMovement>()) {
		return;
	}

	if (!key.empty() && !PlatformerJumpControllerRegistry::Find(key)) {
		return;
	}

	entity.Get<PlatformerMovement>().jump_controller = key;
	SyncPlatformerJumpController(entity);
}

void UpdatePlatformerJumpController(Entity entity, PlatformerJumpContext& ctx) {
	if (!entity || !entity.Has<PlatformerMovement>()) {
		return;
	}

	const auto& movement{ entity.Get<PlatformerMovement>() };
	if (movement.jump_controller.empty()) {
		return;
	}

	const auto* controller{ PlatformerJumpControllerRegistry::Find(movement.jump_controller) };
	if (!controller) {
		return;
	}

	if (!controller->has(entity)) {
		controller->add_default(entity);
	}

	if (controller->has(entity)) {
		controller->update(entity, ctx);
	}
}

} // namespace ptgn
