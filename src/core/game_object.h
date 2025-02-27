#pragma once

#include <array>
#include <unordered_set>

#include "components/transform.h"
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
	using ecs::Entity::Entity;
	GameObject() = default;
	GameObject(const ecs::Entity& e);
	explicit GameObject(ecs::Manager& manager);

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
	GameObject& AddChild(const GameObject& o);

	// @return *this.
	GameObject& RemoveChild(const GameObject& o);

	// If object has no parent, returns *this.
	[[nodiscard]] GameObject GetParent() const;

	[[nodiscard]] bool HasParent() const;

protected:
	friend class impl::RenderData;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;

	GameObject& SetParent(const GameObject& o);
};

struct Children {
	Children(const ecs::Entity& o);

	void Add(const ecs::Entity& o);
	void Remove(const ecs::Entity& o);

	[[nodiscard]] bool IsEmpty() const;

private:
	std::unordered_set<ecs::Entity> children_;
};

[[nodiscard]] bool IsVisible(const GameObject& e);
[[nodiscard]] bool IsEnabled(const GameObject& e);
[[nodiscard]] Transform GetLocalTransform(const GameObject& e);
[[nodiscard]] Transform GetTransform(const GameObject& e);
[[nodiscard]] V2_float GetLocalPosition(const GameObject& e);
[[nodiscard]] V2_float GetPosition(const GameObject& e);
[[nodiscard]] float GetLocalRotation(const GameObject& e);
[[nodiscard]] float GetRotation(const GameObject& e);
[[nodiscard]] V2_float GetLocalScale(const GameObject& e);
[[nodiscard]] V2_float GetScale(const GameObject& e);
[[nodiscard]] Depth GetDepth(const GameObject& e);
[[nodiscard]] BlendMode GetBlendMode(const GameObject& e);
[[nodiscard]] Origin GetOrigin(const GameObject& e);
[[nodiscard]] Color GetTint(const GameObject& e);
[[nodiscard]] GameObject GetParent(const GameObject& e);
[[nodiscard]] bool HasParent(const GameObject& e);
[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const GameObject& e, bool flip_vertically
);

} // namespace ptgn