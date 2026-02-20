#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/component.h"
#include "core/event/event.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Renderer;

namespace impl {

struct Visible {};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct LineWidth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	LineWidth() : ArithmeticComponent{ 1.0f } {}
};

struct EntityDepthCompare {
	EntityDepthCompare() = default;
	explicit EntityDepthCompare(bool ascending);

	bool operator()(Entity a, Entity b) const;

	bool ascending{ true };
};

Entity SetDraw(Entity entity, std::string_view drawable_name);

} // namespace impl

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;
};

void SortByDepth(std::vector<Entity>& entities, bool ascending = true);

/// @return entity.
Entity SetDrawOrigin(Entity entity, Origin origin);

[[nodiscard]] Origin GetDrawOrigin(Entity entity);

/// @return entity.
template <DrawableType T>
Entity SetDraw(Entity entity) {
	return impl::SetDraw(entity, type_name<T>());
}

[[nodiscard]] bool HasDraw(Entity entity);

/// @return entity.
Entity RemoveDraw(Entity entity);

/// @return entity.
Entity SetVisible(Entity entity, bool visible);

/// @return entity.
Entity Show(Entity entity);

/// @return entity.
Entity Hide(Entity entity);

struct EntityShow : public Event<EntityShow> {};

struct EntityHide : public Event<EntityHide> {};

// @return True if the entity is visible, false otherwise. An entity is considered visible if it has
// a Visible component.
[[nodiscard]] bool IsVisible(Entity entity);

/// @return entity.
Entity SetDepth(Entity entity, Depth depth);

[[nodiscard]] Depth GetDepth(Entity entity);

/// @return entity.
Entity SetBlendMode(Entity entity, BlendMode blend_mode);

[[nodiscard]] BlendMode GetBlendMode(Entity entity);

/// color::White will clear tint.
/// @return entity.
Entity SetTint(Entity entity, Color color = color::White);

[[nodiscard]] Color GetTint(Entity entity);

/// @return Unscaled size of the entire texture in pixels.
[[nodiscard]] V2_int GetTextureSize(Entity entity);

/// @return Unscaled size of the cropped texture in pixels.
[[nodiscard]] V2_int GetCroppedSize(Entity entity);

void SetDisplaySize(Entity entity, V2_float display_size);

/// @return Scaled size of the cropped texture in pixels.
[[nodiscard]] V2_float GetDisplaySize(Entity entity);

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically);

namespace impl {

void DrawTexture(
	Renderer& renderer, Texture texture, Transform transform, V2_float size, Origin draw_origin,
	Color tint, BlendMode blend_mode, const std::array<V2_float, 4>& texture_coordinates,
	Entity camera
);

} // namespace impl

} // namespace ptgn