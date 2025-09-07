#include "components/draw.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "common/assert.h"
#include "components/drawable.h"
#include "components/effects.h"
#include "components/generic.h"
#include "components/offsets.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "core/script.h"
#include "core/script_interfaces.h"
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "renderer/text.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

namespace impl {

Entity& SetDrawImpl(Entity& entity, std::string_view drawable_name) {
	EntityAccess::Add<IDrawable>(entity, drawable_name);
	return entity;
}

} // namespace impl

bool HasDraw(const Entity& entity) {
	return entity.Has<impl::IDrawable>();
}

Entity& RemoveDraw(Entity& entity) {
	impl::EntityAccess::Remove<impl::IDrawable>(entity);
	return entity;
}

Entity& SetDrawOffset(Entity& entity, const V2_float& offset) {
	entity.TryAdd<impl::Offsets>().custom.SetPosition(offset);
	return entity;
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, EntityDepthCompare{ ascending });
}

Entity& SetDrawOrigin(Entity& entity, Origin origin) {
	if (entity.Has<Origin>()) {
		entity.Get<Origin>() = origin;
	} else {
		entity.Add<Origin>(origin);
	}
	return entity;
}

Origin GetDrawOrigin(const Entity& entity) {
	return entity.GetOrDefault<Origin>(Origin::Center);
}

Entity& SetVisible(Entity& entity, bool visible) {
	if (visible) {
		impl::EntityAccess::Add<Visible>(entity, visible);
		if (entity.Has<Scripts>()) {
			entity.Get<Scripts>().AddAction(&DrawScript::OnShow);
		}
	} else {
		if (entity.Has<Scripts>()) {
			entity.Get<Scripts>().AddAction(&DrawScript::OnHide);
		}
		impl::EntityAccess::Remove<Visible>(entity);
	}
	return entity;
}

Entity& Show(Entity& entity) {
	return SetVisible(entity, true);
}

Entity& Hide(Entity& entity) {
	return SetVisible(entity, false);
}

bool IsVisible(const Entity& entity) {
	return entity.GetOrDefault<Visible>(false);
}

Entity& SetDepth(Entity& entity, const Depth& depth) {
	if (entity.Has<Depth>()) {
		impl::EntityAccess::Get<Depth>(entity) = depth;
	} else {
		impl::EntityAccess::Add<Depth>(entity, depth);
	}
	return entity;
}

Depth GetDepth(const Entity& entity) {
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

Entity& SetBlendMode(Entity& entity, BlendMode blend_mode) {
	if (entity.Has<BlendMode>()) {
		entity.Get<BlendMode>() = blend_mode;
	} else {
		entity.Add<BlendMode>(blend_mode);
	}
	return entity;
}

BlendMode GetBlendMode(const Entity& entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

Entity& SetTint(Entity& entity, const Color& color) {
	if (color != Tint{}) {
		impl::EntityAccess::Add<Tint>(entity, color);
	} else {
		impl::EntityAccess::Remove<Tint>(entity);
	}
	return entity;
}

Color GetTint(const Entity& entity) {
	return entity.GetOrDefault<Tint>();
}

namespace impl {

V2_int GetTextureSize(const Entity& entity) {
	V2_int size;
	if (entity.Has<TextureSize>()) {
		size = V2_int{ entity.Get<TextureSize>() };
		if (!size.IsZero()) {
			return size;
		}
	}
	if (entity.Has<TextureHandle>()) {
		size = entity.Get<TextureHandle>().GetSize(entity);
	}

	PTGN_ASSERT(!size.IsZero(), "Texture does not have a valid size");
	return size;
}

V2_int GetCroppedSize(const Entity& entity) {
	if (entity.Has<TextureCrop>()) {
		const auto& crop{ entity.Get<TextureCrop>() };
		return crop.size;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity& entity, const V2_float& display_size) {
	auto& texture_size{ entity.TryAdd<TextureSize>() };
	texture_size = display_size;
}

V2_float GetDisplaySize(const Entity& entity) {
	if (!entity.Has<TextureHandle>() && !entity.Has<TextureCrop>()) {
		return {};
	}
	return GetCroppedSize(entity) * GetScale(entity);
}

std::array<V2_float, 4> GetTextureCoordinates(const Entity& entity, bool flip_vertically) {
	auto tex_coords{ GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			FlipTextureCoordinates(tex_coords, Flip::Vertical);
		}
	};

	if (!entity) {
		check_vertical_flip();
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize(entity) };

	if (texture_size.IsZero()) {
		check_vertical_flip();
		return tex_coords;
	}

	if (entity.Has<TextureCrop>()) {
		const auto& crop{ entity.Get<TextureCrop>() };
		if (crop != TextureCrop{}) {
			tex_coords = GetTextureCoordinates(crop.position, crop.size, texture_size);
		}
	}

	auto scale{ GetScale(entity) };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (entity.Has<Flip>()) {
		FlipTextureCoordinates(tex_coords, entity.Get<Flip>());
	}

	check_vertical_flip();

	return tex_coords;
}

} // namespace impl

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

EntityDepthCompare::EntityDepthCompare(bool ascending) : ascending{ ascending } {}

bool EntityDepthCompare::operator()(const Entity& a, const Entity& b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return ascending ? a.WasCreatedBefore(b) : !a.WasCreatedBefore(b);
	}
	return ascending ? (depth_a < depth_b) : (depth_a > depth_b);
}

namespace impl {

void DrawTexture(RenderData& ctx, const Entity& entity, bool flip_texture) {
	ShapeDrawInfo info{ entity };

	Sprite sprite{ entity };
	const auto& texture{ sprite.GetTexture() };
	Rect rect{ sprite.GetSize() };
	auto texture_coordinates{ sprite.GetTextureCoordinates(flip_texture) };

	auto origin{ GetDrawOrigin(entity) };
	auto pre_fx{ entity.GetOrDefault<PreFX>() };

	ctx.AddTexturedQuad(
		info.transform, texture, rect, origin, info.tint, info.depth, texture_coordinates,
		info.state, pre_fx
	);
}

void DrawText(
	RenderData& ctx, Text text, const V2_int& text_size, const Camera& camera,
	const Color& additional_tint, Origin offset_origin, const V2_float& offset_size
) {
	if (!text.Has<TextContent>()) {
		return;
	}

	if (text.Get<TextContent>().GetValue().empty()) {
		return;
	}

	if (text.Has<TextColor>() && text.Get<TextColor>().a == 0) {
		return;
	}

	impl::ShapeDrawInfo info{ text };

	if (info.tint.a == 0 || additional_tint.a == 0) {
		return;
	}

	if (camera) {
		info.state.camera = camera;
	}

	// Offset text so it is centered on the offset origin and size.
	auto offset{ -GetOriginOffset(offset_origin, offset_size * Abs(info.transform.GetScale())) };
	info.transform.Translate(offset);

	if (bool is_hd{ text.IsHD() }) {
		auto scene_scale{ text.GetScene().GetScaleRelativeTo(text.GetCamera()) };

		info.transform.Scale(info.transform.GetScale() / scene_scale);

		if (text.GetFontSize(is_hd) != text.Get<impl::CachedFontSize>()) {
			text.RecreateTexture();
		}
	}

	auto origin{ GetDrawOrigin(text) };

	auto texture_coordinates{ Sprite{ text }.GetTextureCoordinates(false) };

	auto pre_fx{ text.GetOrDefault<PreFX>() };

	Color text_tint{ additional_tint.Normalized() * info.tint.Normalized() };

	const auto& text_texture{ text.GetTexture() };

	if (!text_texture.IsValid()) {
		return;
	}

	V2_int size{ text_size };

	// If the text texture size for any text_size dimension that is zero.
	if (size.HasZero()) {
		V2_int texture_size{ text_texture.GetSize() };
		if (!size.x) {
			size.x = texture_size.x;
		}
		if (!size.y) {
			size.y = texture_size.y;
		}
	}

	ctx.AddTexturedQuad(
		info.transform, text_texture, Rect{ size }, origin, text_tint, info.depth,
		texture_coordinates, info.state, pre_fx
	);
}

void DrawText(RenderData& ctx, const Entity& entity) {
	impl::DrawText(ctx, entity, V2_float{}, Camera{}, color::White, Origin::Center, V2_float{});
}

void DrawRect(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Rect>());

	ShapeDrawInfo info{ entity };

	const auto& rect{ entity.Get<Rect>() };
	auto origin{ GetDrawOrigin(entity) };

	ctx.AddQuad(info.transform, rect, origin, info.tint, info.depth, info.line_width, info.state);
}

void DrawRoundedRect(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<RoundedRect>());

	ShapeDrawInfo info{ entity };
	info.state.shader_pass = game.shader.Get("rounded_rect");

	const auto& rrect{ entity.Get<RoundedRect>() };
	auto origin{ GetDrawOrigin(entity) };

	ctx.AddRoundedQuad(
		info.transform, rrect, origin, info.tint, info.depth, info.line_width, info.state
	);
}

void DrawArc(RenderData& ctx, const Entity& entity, bool clockwise) {
	PTGN_ASSERT(entity.Has<Arc>());

	ShapeDrawInfo info{ entity };
	info.state.shader_pass = game.shader.Get("arc");

	const auto& arc{ entity.Get<Arc>() };

	ctx.AddArc(info.transform, arc, clockwise, info.tint, info.depth, info.line_width, info.state);
}

void DrawCapsule(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Capsule>());

	ShapeDrawInfo info{ entity };
	info.state.shader_pass = game.shader.Get("capsule");

	const auto& capsule{ entity.Get<Capsule>() };

	ctx.AddCapsule(info.transform, capsule, info.tint, info.depth, info.line_width, info.state);
}

void DrawCircle(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Circle>());

	ShapeDrawInfo info{ entity };
	info.state.shader_pass = game.shader.Get("circle");

	const auto& circle{ entity.Get<Circle>() };

	ctx.AddCircle(info.transform, circle, info.tint, info.depth, info.line_width, info.state);
}

void DrawEllipse(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Ellipse>());

	ShapeDrawInfo info{ entity };
	info.state.shader_pass = game.shader.Get("circle"); // ellipses use the circle shader.

	const auto& ellipse{ entity.Get<Ellipse>() };

	ctx.AddEllipse(info.transform, ellipse, info.tint, info.depth, info.line_width, info.state);
}

void DrawLine(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Line>());

	ShapeDrawInfo info{ entity };

	const auto& line{ entity.Get<Line>() };

	ctx.AddLine(info.transform, line, info.tint, info.depth, info.line_width, info.state);
}

void DrawPolygon(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Polygon>());

	ShapeDrawInfo info{ entity };

	const auto& polygon{ entity.Get<Polygon>() };

	ctx.AddPolygon(info.transform, polygon, info.tint, info.depth, info.line_width, info.state);
}

void DrawTriangle(RenderData& ctx, const Entity& entity) {
	PTGN_ASSERT(entity.Has<Triangle>());

	ShapeDrawInfo info{ entity };

	const auto& triangle{ entity.Get<Triangle>() };

	ctx.AddTriangle(info.transform, triangle, info.tint, info.depth, info.line_width, info.state);
}

} // namespace impl

Entity CreateRect(
	Manager& manager, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
) {
	auto rect{ manager.CreateEntity() };

	SetDraw<Rect>(rect);
	Show(rect);

	SetPosition(rect, position);
	rect.Add<Rect>(size);
	SetDrawOrigin(rect, origin);

	SetTint(rect, color);
	rect.Add<LineWidth>(line_width);

	return rect;
}

Entity CreatePolygon(
	Manager& manager, const V2_float& position, const std::vector<V2_float>& vertices,
	const Color& color, float line_width
) {
	auto polygon{ manager.CreateEntity() };

	SetDraw<Polygon>(polygon);
	Show(polygon);

	SetPosition(polygon, position);
	polygon.Add<Polygon>(vertices);

	SetTint(polygon, color);
	polygon.Add<LineWidth>(line_width);

	return polygon;
}

Entity CreateCircle(
	Manager& manager, const V2_float& position, float radius, const Color& color, float line_width
) {
	auto circle{ manager.CreateEntity() };

	SetDraw<Circle>(circle);
	Show(circle);

	SetPosition(circle, position);
	circle.Add<Circle>(radius);

	SetTint(circle, color);
	circle.Add<LineWidth>(line_width);

	return circle;
}

} // namespace ptgn