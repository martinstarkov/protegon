#pragma once

#include <array>
#include <concepts>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/assert.h"

#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/vector2.h"
#include "core/util/concepts.h"
#include "core/util/type_info.h"
#include "renderer/pipeline/blend_mode.h"
#include "core/graphics/color.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"

namespace ptgn {

class DrawContext;

inline constexpr float kMinLineWidth{ 1.0f };

struct Solid {
	constexpr Solid() = default;
};

struct Hollow {
	constexpr Hollow() = default;

	constexpr Hollow(float line_width) : line_width{ line_width } { // NOSONAR
		PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least {}", kMinLineWidth);
	}

	float line_width{ kMinLineWidth }; // must be positive and >= kMinLineWidth
};

struct FillStyle {
	constexpr FillStyle() = default;

	constexpr FillStyle(float line_width) : style{ Hollow{ line_width } } { // NOSONAR
	}

	constexpr FillStyle(Solid) : style{ Solid{} } {} // NOSONAR

	template <typename F>
	decltype(auto) Visit(F&& f) const {
		return std::visit(std::forward<F>(f), style);
	}

	template <Invocable SolidFn, Invocable<float> HollowFn>
	auto Apply(SolidFn&& solid_fn, HollowFn&& hollow_fn) {
		using R1 = std::invoke_result_t<SolidFn>;
		using R2 = std::invoke_result_t<HollowFn, float>;

		if constexpr (std::same_as<R1, R2>) {
			return Visit([&]<typename T>(const T& s) -> R1 {
				if constexpr (std::is_same_v<T, Solid>) {
					return solid_fn();
				} else if constexpr (std::is_same_v<T, Hollow>) {
					PTGN_ASSERT(s.line_width >= kMinLineWidth);
					return hollow_fn(s.line_width);
				} else {
					static_assert(false, "Incomplete visitor");
				}
			});
		} else {
			using R = std::variant<R1, R2>;

			return Visit([&]<typename T>(const T& s) -> R {
				if constexpr (std::is_same_v<T, Solid>) {
					return R{ solid_fn() };
				} else if constexpr (std::is_same_v<T, Hollow>) {
					PTGN_ASSERT(s.line_width >= kMinLineWidth);
					return R{ hollow_fn(s.line_width) };
				} else {
					static_assert(false, "Incomplete visitor");
				}
			});
		}
	}

private:
	friend class DrawContext;

	/// @brief Converts a fill style to a SDF line thickness for shaders to draw hollow and solid
	/// shapes.
	[[nodiscard]] float NormalizedToSDFThickness(float fade, V2_float radii) const;

	std::variant<Hollow, Solid> style{ Solid{} };
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

namespace event {

/// @brief This event is emitted when an entity becomes visible, either through Show() or
/// SetVisible(true).
struct EntityShow : public Event<EntityShow> {
	EntityShow() = default;
};

/// @brief This event is emitted when an entity becomes hidden, either through Hide() or
/// SetVisible(false).
struct EntityHide : public Event<EntityHide> {
	EntityHide() = default;
};

} // namespace event

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