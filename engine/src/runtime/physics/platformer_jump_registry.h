#pragma once

#include <concepts>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/math/transform.h"
#include "core/util/hash.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
struct PlatformerMovement;
struct RigidBody;

struct PlatformerJumpContext {
	Entity entity{};
	Scene& scene;
	PlatformerMovement& platformer;
	Transform& transform;
	RigidBody& rigid_body;
	V2_float gravity{};
	secondsf dt{};
};

struct RegisteredPlatformerJumpController {
	std::string key{};
	std::string label{};
	std::string group{};
	std::string description{};
	std::size_t component_hash{ 0 };
	bool (*has)(Entity){ nullptr };
	void* (*get)(Entity){ nullptr };
	void (*add_default)(Entity){ nullptr };
	void (*remove)(Entity){ nullptr };
	void (*update)(Entity, PlatformerJumpContext&){ nullptr };
};

class PlatformerJumpControllerRegistry {
public:
	static void Register(RegisteredPlatformerJumpController controller);

	[[nodiscard]] static const RegisteredPlatformerJumpController* Find(std::string_view key);

	[[nodiscard]] static const std::vector<RegisteredPlatformerJumpController>& Controllers();
};

template <typename T>
concept PlatformerJumpControllerType = std::default_initializable<T> &&
	requires(T& controller, PlatformerJumpContext& ctx) {
		controller.Update(ctx);
	};

template <PlatformerJumpControllerType T>
class AutoPlatformerJumpControllerRegistration {
public:
	AutoPlatformerJumpControllerRegistration(
		std::string key, std::string label, std::string group, std::string description
	) {
		PlatformerJumpControllerRegistry::Register(RegisteredPlatformerJumpController{
			.key = std::move(key),
			.label = std::move(label),
			.group = std::move(group),
			.description = std::move(description),
			.component_hash = Hash<T>(),
			.has = [](Entity entity) { return entity.Has<T>(); },
			.get = [](Entity entity) -> void* {
				return entity.Has<T>() ? std::addressof(entity.Get<T>()) : nullptr;
			},
			.add_default = [](Entity entity) {
				if (!entity.Has<T>()) {
					entity.Add<T>();
				}
			},
			.remove = [](Entity entity) {
				if (entity.Has<T>()) {
					entity.Remove<T>();
				}
			},
			.update = [](Entity entity, PlatformerJumpContext& ctx) {
				entity.Get<T>().Update(ctx);
			},
		});
	}
};

void SetPlatformerJumpController(Entity entity, std::string_view key);
void ClearPlatformerJumpController(Entity entity);
void SyncPlatformerJumpController(Entity entity);
void UpdatePlatformerJumpController(Entity entity, PlatformerJumpContext& ctx);

#define PTGN_PLATFORMER_JUMP_CONCAT_IMPL(a, b) a##b
#define PTGN_PLATFORMER_JUMP_CONCAT(a, b) PTGN_PLATFORMER_JUMP_CONCAT_IMPL(a, b)
#define PTGN_REGISTER_PLATFORMER_JUMP(Type, Key, Label, Group, Description)                   \
	[[maybe_unused]] const ptgn::AutoPlatformerJumpControllerRegistration<Type>             \
		PTGN_PLATFORMER_JUMP_CONCAT(kPlatformerJumpRegistration_, __COUNTER__){                 \
			Key, Label, Group, Description                                                       \
		}

} // namespace ptgn
