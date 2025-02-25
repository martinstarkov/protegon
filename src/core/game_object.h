#pragma once

#include <array>
#include <type_traits>
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

[[nodiscard]] bool IsVisible(const ecs::Entity& e);
[[nodiscard]] bool IsEnabled(const ecs::Entity& e);
[[nodiscard]] V2_float GetLocalPosition(const ecs::Entity& e);
[[nodiscard]] V2_float GetPosition(const ecs::Entity& e);
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
[[nodiscard]] V2_float GetRotationCenter(const ecs::Entity& e);
[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(
	const ecs::Entity& e, bool flip_vertically
);

// A game object is a owning wrapper around an ecs::Entity.
class GameObject {
public:
	GameObject() = default;
	GameObject(const ecs::Entity& e);
	GameObject(const GameObject&)				 = default;
	GameObject& operator=(const GameObject&)	 = default;
	GameObject(GameObject&&) noexcept			 = default;
	GameObject& operator=(GameObject&&) noexcept = default;
	virtual ~GameObject();

	bool operator==(const ecs::Entity& o) const;
	bool operator!=(const ecs::Entity& o) const;

	operator ecs::Entity() const;

	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		if constexpr (std::is_convertible_v<T*, GameObject*>) {
			auto child{ entity.GetManager().CreateEntity() };
			auto& component{ entity.Add<T>(child, std::forward<Ts>(constructor_args)...) };
			AddChild(child);
			component.SetParent(entity);
			return component;
		} else {
			return entity.Add<T>(std::forward<Ts>(constructor_args)...);
		}
	}

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

	[[nodiscard]] V2_float GetRotationCenter() const;

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

	ecs::Entity entity;

protected:
	friend class impl::RenderData;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;

	GameObject& SetParent(const ecs::Entity& o);
};

struct Children {
	Children(const ecs::Entity& o);

	void Add(const ecs::Entity& o);
	void Remove(const ecs::Entity& o);

	[[nodiscard]] bool IsEmpty() const;

private:
	std::unordered_set<ecs::Entity> children_;
};

} // namespace ptgn