#pragma once

#include <array>
#include <cstdint>
#include <span>
#include <string_view>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class RenderContext;

inline constexpr float kMinLineWidth{ 1.0f };

namespace impl {

struct Solid {};

struct Hollow {
	float line_width{ kMinLineWidth }; // must be positive and >= kMinLineWidth
};

} // namespace impl

struct FillStyle {
	FillStyle() = default;
	FillStyle(float line_width);

	static FillStyle Hollow(float line_width = 1.0f);
	static FillStyle Solid();

	std::variant<impl::Hollow, impl::Solid> style{ impl::Hollow{} };

private:
	FillStyle(impl::Solid);
};

struct Depth : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;
};

namespace impl {

void DrawQuadTexture(
	RenderContext& renderer, Texture texture, Transform transform, V2_float size,
	Origin draw_origin, Color tint, Depth depth, BlendMode blend_mode,
	const std::array<V2_float, 4>& texture_coordinates
);

void DrawLines(
	RenderContext& renderer, std::span<const V2_float> points, float line_width,
	const Transform& transform, Color tint, float depth, BlendMode blend_mode
);

void DrawShape(
	RenderContext& renderer, const Shape& shape, Transform transform, Color tint,
	FillStyle fill_style, Origin draw_origin, Depth depth_component, BlendMode blend_mode
);

template <ShapeType T>
void DrawShape(RenderContext& renderer, Entity entity);

struct Visible {};

struct Tint : public ColorComponent {
	using ColorComponent::ColorComponent;

	Tint() : ColorComponent{ color::White } {}
};

struct EntityDepthCompare {
	EntityDepthCompare() = default;
	explicit EntityDepthCompare(bool ascending);

	bool operator()(Entity a, Entity b) const;

	bool ascending{ true };
};

void SetDraw(Entity entity, std::string_view drawable_name);

struct CapsuleDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct CircleDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct EllipseDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct ArcDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct PolygonDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct RectDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct RoundedRectDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct TriangleDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

struct LineDraw {
	static void Draw(RenderContext& renderer, Entity entity);
};

} // namespace impl

void SortByDepth(std::vector<Entity>& entities, bool ascending = true);

void SetDrawOrigin(Entity entity, Origin origin);

[[nodiscard]] Origin GetDrawOrigin(Entity entity);

template <DrawableType T>
void SetDraw(Entity entity) {
	impl::SetDraw(entity, type_name<T>());
}

[[nodiscard]] bool HasDraw(Entity entity);

void RemoveDraw(Entity entity);

void SetVisible(Entity entity, bool visible, bool emit_visibility_event = true);

void Show(Entity entity, bool emit_visibility_event = true);

void Hide(Entity entity, bool emit_visibility_event = true);

struct EntityShow : public Event<EntityShow> {};

struct EntityHide : public Event<EntityHide> {};

/// @return True if the entity is visible, false otherwise.
[[nodiscard]] bool IsVisible(Entity entity);

void SetDepth(Entity entity, Depth depth);

[[nodiscard]] Depth GetDepth(Entity entity);

void SetBlendMode(Entity entity, BlendMode blend_mode);

[[nodiscard]] BlendMode GetBlendMode(Entity entity);

/// @param color color::White will clear any tint.
void SetTint(Entity entity, Color color = color::White);

[[nodiscard]] Color GetTint(Entity entity);

/// @brief Set the size of the texture to be drawn. This will stretch the texture to fit the given
/// size.
void SetTextureSize(Entity entity, V2_float size);

/// @return Unscaled size of the entire texture in pixels.
[[nodiscard]] V2_int GetTextureSize(Entity entity);

/// @return Unscaled size of the cropped texture in pixels.
[[nodiscard]] V2_int GetCroppedTextureSize(Entity entity);

void SetDisplaySize(Entity entity, V2_float display_size);

/// @return Scaled size of the cropped texture in pixels.
[[nodiscard]] V2_float GetDisplaySize(Entity entity);

[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically);

PTGN_REGISTER_DRAWABLE(impl::CapsuleDraw);
PTGN_REGISTER_DRAWABLE(impl::CircleDraw);
PTGN_REGISTER_DRAWABLE(impl::EllipseDraw);
PTGN_REGISTER_DRAWABLE(impl::ArcDraw);
PTGN_REGISTER_DRAWABLE(impl::PolygonDraw);
PTGN_REGISTER_DRAWABLE(impl::RectDraw);
PTGN_REGISTER_DRAWABLE(impl::RoundedRectDraw);
PTGN_REGISTER_DRAWABLE(impl::TriangleDraw);
PTGN_REGISTER_DRAWABLE(impl::LineDraw);

} // namespace ptgn