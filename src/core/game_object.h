#pragma once

#include <array>
#include <unordered_set>

#include "components/common.h"
#include "core/transform.h"
#include "ecs/ecs.h"
#include "math/vector2.h"
#include "renderer/blend_mode.h"
#include "renderer/color.h"
#include "renderer/origin.h"

namespace ptgn {

namespace impl {

class RenderData;

} // namespace impl

class GameObject : public ecs::Entity {
public:
	GameObject() = default;
	GameObject(ecs::Entity&& entity);
	explicit GameObject(ecs::Manager& manager);
	GameObject(const GameObject&)				 = delete;
	GameObject& operator=(const GameObject&)	 = delete;
	GameObject(GameObject&&) noexcept			 = default;
	GameObject& operator=(GameObject&&) noexcept = default;
	virtual ~GameObject();

	operator ecs::Entity() const;

	[[nodiscard]] ecs::Entity GetEntity() const;

	// @return *this.
	GameObject& SetVisible(bool visible);

	// @return *this.
	GameObject& Show();

	// @return *this.
	GameObject& Hide();

	// @return *this.
	GameObject& SetEnabled(bool enabled);

	// @return *this.
	GameObject& Disable();

	// @return *this.
	GameObject& Enable();

	[[nodiscard]] bool IsVisible() const;
	[[nodiscard]] bool IsEnabled() const;

	[[nodiscard]] Transform GetLocalTransform() const;
	[[nodiscard]] Transform GetTransform() const;

	[[nodiscard]] V2_float GetLocalPosition() const;
	[[nodiscard]] V2_float GetPosition() const;

	// @return Rotation of the object in radians relative to { 1, 0 }, clockwise positive.
	[[nodiscard]] float GetLocalRotation() const;

	// @return Rotation of the object in radians relative to its parent object and { 1, 0 },
	// clockwise positive.
	[[nodiscard]] float GetRotation() const;

	[[nodiscard]] V2_float GetLocalScale() const;
	[[nodiscard]] V2_float GetScale() const;

	// Set the local position of this game object.
	// @return *this.
	GameObject& SetPosition(const V2_float& position);

	// Set the local rotation of this game object.
	// @return *this.
	GameObject& SetRotation(float rotation);

	// Set the local scale of this game object.
	// @return *this.
	GameObject& SetScale(const V2_float& scale);

	GameObject& SetDepth(const Depth& depth);

	[[nodiscard]] Depth GetDepth() const;

	GameObject& SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] BlendMode GetBlendMode() const;

	// @return *this.
	GameObject& AddChild(const ecs::Entity& o);

	// @return *this.
	GameObject& RemoveChild(const ecs::Entity& o);

	// If object has no parent, returns *this.
	[[nodiscard]] ecs::Entity GetParent() const;

	[[nodiscard]] bool HasParent() const;

	GameObject& SetTint(const Color& color);
	[[nodiscard]] Color GetTint() const;

	GameObject& SetParent(const ecs::Entity& o);

protected:
	friend class impl::RenderData;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;
};

struct Children {
	Children(const ecs::Entity& o);

	void Add(const ecs::Entity& o);
	void Remove(const ecs::Entity& o);

	[[nodiscard]] bool IsEmpty() const;

private:
	std::unordered_set<ecs::Entity> children_;
};

namespace impl {

Transform* GetAbsoluteTransformImpl(const ecs::Entity& e);

} // namespace impl

[[nodiscard]] bool IsVisible(const ecs::Entity& e);
[[nodiscard]] bool IsEnabled(const ecs::Entity& e);
// @return Reference to the transform of the top most parent entity. If none of the entities have a
// transform component, an assertion is called.
[[nodiscard]] Transform& GetAbsoluteTransform(const ecs::Entity& e);
[[nodiscard]] Transform GetLocalTransform(const ecs::Entity& e);
[[nodiscard]] Transform GetTransform(const ecs::Entity& e);
[[nodiscard]] V2_float GetLocalPosition(const ecs::Entity& e);
[[nodiscard]] V2_float GetPosition(const ecs::Entity& e);
[[nodiscard]] Transform GetLocalOffsetTransform(const ecs::Entity& e);
[[nodiscard]] Transform GetOffsetTransform(const ecs::Entity& e);
[[nodiscard]] float GetLocalRotation(const ecs::Entity& e);
[[nodiscard]] float GetRotation(const ecs::Entity& e);
[[nodiscard]] V2_float GetLocalScale(const ecs::Entity& e);
[[nodiscard]] V2_float GetScale(const ecs::Entity& e);
[[nodiscard]] Depth GetDepth(const ecs::Entity& e);
[[nodiscard]] BlendMode GetBlendMode(const ecs::Entity& e);
[[nodiscard]] Origin GetOrigin(const ecs::Entity& e);
[[nodiscard]] Color GetTint(const ecs::Entity& e);
[[nodiscard]] ecs::Entity GetParent(const ecs::Entity& e);
[[nodiscard]] bool HasParent(const ecs::Entity& e);
[[nodiscard]] bool IsImmovable(const ecs::Entity& e);
[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const ecs::Entity& e, bool flip_vertically
);

} // namespace ptgn