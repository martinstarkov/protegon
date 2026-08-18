#pragma once

#include <optional>

#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/time.h"
#include "serialization/serialize.h"

namespace ptgn {

class Scene;
class SceneContext;

enum class BoundaryBehavior {
	/// @brief Clamp position and stop velocity.
	StopVelocity,
	/// @brief Clamp position and do not change velocity.
	SlideVelocity,
	/// @brief Bounce off bounds by flipping velocity.
	ReflectVelocity
};
PTGN_REFLECT_ENUM(BoundaryBehavior);

struct Bounds {
	V2_float position;
	V2_float size;
	BoundaryBehavior behavior{ BoundaryBehavior::SlideVelocity };

	PTGN_REFLECT(Bounds, position, size, behavior)
};

class Physics {
public:
	std::optional<Bounds> GetBounds() const;
	/// @param bounds Nullopt results in no boundary enforcement.
	void SetBounds(std::optional<Bounds> bounds = std::nullopt);

	V2_float GetGravity() const;
	void SetGravity(V2_float gravity);

	[[nodiscard]] secondsf dt() const;

	void SetEnabled(bool enabled = true);
	void Disable();
	void Enable();

	/// @return True if physics is enabled, false otherwise.
	[[nodiscard]] bool IsEnabled() const;

	PTGN_REFLECT(Physics, gravity_, bounds_, enabled_)

private:
	friend class Scene;
	friend class SceneContext;

	Physics() = delete;
	explicit Physics(Scene& scene);
	~Physics() noexcept = default;
	Physics(const Physics&) = delete;
	Physics& operator=(const Physics&) = delete;
	Physics(Physics&&) noexcept = delete;
	Physics& operator=(Physics&&) noexcept = delete;

	void PreCollisionUpdate() const;
	void PostCollisionUpdate() const;
	void UpdatePlatformerGrounding() const;

	static void HandleBoundary(Transform& transform, V2_float& velocity, const Bounds& bounds);

	void Rebind(Scene& scene);
	void Reset();

	Scene* scene_{ nullptr };
	bool enabled_{ true };
	std::optional<Bounds> bounds_;
	V2_float gravity_{ 0.0f, 0.0f };
};

} // namespace ptgn
