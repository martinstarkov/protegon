#include "runtime/graphics/draw.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

#include "core/assert.h"
#include "core/log.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/shape.h"
#include "core/math/geometry/triangle.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/texture.h"
#include "renderer/primitives/vertex.h"
#include "runtime/ecs/component.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/render_context.h"
#include "runtime/graphics/sprite.h"
#include "runtime/scene/scene.h"

namespace ptgn {

FillStyle::FillStyle(float line_width) : style{ Hollow{ line_width } } {
	PTGN_ASSERT(line_width >= kMinLineWidth, "Line width must be at least ", kMinLineWidth);
}

FillStyle::FillStyle(Solid) : style{ Solid{} } {}

float FillStyle::NormalizedToSDFThickness(float fade, V2_float radii) const {
	return Visit([fade, radii]<typename T>(const T& s) {
		if constexpr (std::is_same_v<T, Solid>) {
			// Internally line width for a filled SDF is 1.0f.
			return 1.0f;
		} else if constexpr (std::is_same_v<T, Hollow>) {
			PTGN_ASSERT(s.line_width >= kMinLineWidth, "Invalid line width for circle");

			// Internally line width for a completely hollow ellipse is 0.0f.
			return fade + s.line_width / std::min(radii.x, radii.y);
		} else {
			static_assert(false, "Incomplete visitor!");
		}
	});
}

namespace impl {

void SetDraw(Entity entity, std::string_view drawable_name) {
	entity.Add<IDrawable>(drawable_name);
}

EntityDepthCompare::EntityDepthCompare(bool ascending) : ascending{ ascending } {}

bool EntityDepthCompare::operator()(Entity a, Entity b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return ascending ? a.WasCreatedBefore(b) : !a.WasCreatedBefore(b);
	}
	return ascending ? (depth_a < depth_b) : (depth_a > depth_b);
}

template <ShapeType T>
void DrawShape(DrawContext& renderer, Entity entity, Camera) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	const auto& shape{ entity.Get<T>() };
	auto draw_transform{ GetDrawTransform(entity) };
	auto tint{ GetTint(entity) };
	auto fill_style{ entity.GetOrDefault<FillStyle>() };
	auto draw_origin{ GetDrawOrigin(entity) };
	auto depth{ GetDepth(entity) };
	auto blend_mode{ GetBlendMode(entity) };

	renderer.DrawShape(shape, draw_transform, tint, fill_style, draw_origin, depth, blend_mode);
}

void CapsuleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Capsule>(renderer, entity, camera);
}

void CircleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Circle>(renderer, entity, camera);
}

void EllipseDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Ellipse>(renderer, entity, camera);
}

void ArcDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Arc>(renderer, entity, camera);
}

void PolygonDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Polygon>(renderer, entity, camera);
}

void RectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Rect>(renderer, entity, camera);
}

void RoundedRectDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<RoundedRect>(renderer, entity, camera);
}

void TriangleDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Triangle>(renderer, entity, camera);
}

void LineDraw::Draw(DrawContext& renderer, Entity entity, Camera camera) {
	DrawShape<Line>(renderer, entity, camera);
}

} // namespace impl

bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>();
}

void RemoveDraw(Entity entity) {
	entity.Remove<impl::IDrawable>();
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, impl::EntityDepthCompare{ ascending });
}

void SetDrawOrigin(Entity entity, Origin origin) {
	entity.Add<Origin>(origin);
}

Origin GetDrawOrigin(Entity entity) {
	return entity.GetOrDefault<Origin>(Origin::Center);
}

void SetVisible(Entity entity, bool visible, bool emit_visibility_event) {
	if (visible) {
		if (entity.Has<impl::Visible>()) {
			return;
		}
		entity.Add<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			PushEvent<event::EntityShow>(entity);
		}
	} else {
		if (!entity.Has<impl::Visible>()) {
			return;
		}
		entity.Remove<impl::Visible>();
		if (emit_visibility_event && entity.HasScene()) {
			PushEvent<event::EntityHide>(entity);
		}
	}
}

void Show(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, true, emit_visibility_event);
}

void Hide(Entity entity, bool emit_visibility_event) {
	SetVisible(entity, false, emit_visibility_event);
}

bool IsVisible(Entity entity) {
	return entity.Has<impl::Visible>();
}

void SetDepth(Entity entity, Depth depth) {
	entity.Add<Depth>(depth);
}

Depth GetDepth(Entity entity) {
	// TODO: This was causing a bug with the mitosis disk background (rock texture) thing in GMTK
	// 2025. Figure out how to fix relative depths.
	/*Depth parent_depth{};
	if (HasParent(entity)) {
		auto parent{ GetParent(entity) };
		if (parent != entity && parent.Has<Depth>()) {
			parent_depth = GetDepth(parent);
		}
	}
	return parent_depth +*/
	return entity.GetOrDefault<Depth>();
}

void SetBlendMode(Entity entity, BlendMode blend_mode) {
	entity.Add<BlendMode>(blend_mode);
}

BlendMode GetBlendMode(Entity entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

void SetTint(Entity entity, Color color) {
	if (color != impl::Tint{}) {
		entity.Add<impl::Tint>(color);
	} else {
		entity.Remove<impl::Tint>();
	}
}

Color GetTint(Entity entity) {
	return entity.GetOrDefault<impl::Tint>();
}

std::optional<V2_int> GetTextureSize(Entity entity) {
	if (auto texture{ entity.TryGet<Texture>() }) {
		auto size{ texture->GetSize() };
		PTGN_ASSERT(!size.IsZero(), "Texture does not have a valid size");
		return size;
	}
	return std::nullopt;
}

std::optional<V2_int> GetCroppedTextureSize(Entity entity) {
	if (auto crop{ entity.TryGet<impl::TextureCrop>() }) {
		if (!crop->size.has_value()) {
			return GetTextureSize(entity);
		}
		PTGN_ASSERT(!crop->size->IsZero(), "Cropped texture does not have a valid size");
		return crop->size;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity entity, V2_float display_size) {
	entity.Add<impl::TextureSize>(display_size);
}

std::optional<V2_float> GetDisplaySize(Entity entity) {
	if (auto texture_size{ entity.TryGet<impl::TextureSize>() }) {
		return texture_size->GetValue();
	}
	auto cropped_size{ GetCroppedTextureSize(entity) };
	if (cropped_size.has_value()) {
		return *cropped_size * GetWorldScale(entity);
	}
	return std::nullopt;
}

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically) {
	if (!entity) {
		return impl::GetDefaultTextureCoordinates(flip_vertically);
	}

	auto texture_size{ GetTextureSize(entity) };

	if (!texture_size.has_value()) {
		return impl::GetDefaultTextureCoordinates(flip_vertically);
	}

	std::array<V2_float, 4> tex_coords;

	if (auto crop{ entity.TryGet<impl::TextureCrop>() }) {
		auto crop_size{ crop->size.value_or(*texture_size) };
		tex_coords = impl::GetTextureCoordinates(
			crop->position, crop_size, *texture_size, flip_vertically, true
		);
	} else {
		tex_coords =
			impl::GetTextureCoordinates({}, *texture_size, *texture_size, flip_vertically, true);
	}

	auto scale{ GetWorldScale(entity) };

	impl::FlipTextureCoordinates(tex_coords, scale);

	return tex_coords;
}

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

} // namespace ptgn