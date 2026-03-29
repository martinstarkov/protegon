#pragma once

#include <array>
#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "core/event/event.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

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

struct Depth : public ArithmeticComponent<float> {
	using ArithmeticComponent::ArithmeticComponent;

	[[nodiscard]] Depth RelativeTo(Depth parent) const;
};

namespace impl {

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
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct CircleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct EllipseDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct ArcDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct PolygonDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct RoundedRectDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct TriangleDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

struct LineDraw {
	static void Draw(DrawContext& renderer, Entity entity, Camera camera);
};

} // namespace impl

void SortByDepth(std::vector<Entity>& entities, bool ascending = true);

void SetDrawOrigin(Entity entity, Origin origin);

Origin GetDrawOrigin(Entity entity);

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

Depth GetDepth(Entity entity);

void SetBlendMode(Entity entity, BlendMode blend_mode);

BlendMode GetBlendMode(Entity entity);

/// @param color color::White will clear any tint.
void SetTint(Entity entity, Color color = color::White);

Color GetTint(Entity entity);

/// @return Unscaled size of the entire texture in pixels.
std::optional<V2_int> GetTextureSize(Entity entity);

/// @return Unscaled size of the cropped texture in pixels.
std::optional<V2_int> GetCroppedTextureSize(Entity entity);

/// @return Scaled size of the cropped texture in pixels.
std::optional<V2_float> GetDisplaySize(Entity entity);

/// @brief Overrides the scale of the entity.
void SetDisplaySize(Entity entity, V2_float display_size);

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically);

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