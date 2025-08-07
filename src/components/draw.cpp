#include "components/draw.h"

#include <array>
#include <functional>
#include <string_view>

#include "common/assert.h"
#include "components/drawable.h"
#include "components/effects.h"
#include "components/offsets.h"
#include "components/sprite.h"
#include "components/transform.h"
#include "core/entity.h"
#include "core/game.h"
#include "core/manager.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/api/origin.h"
#include "renderer/render_data.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"

namespace ptgn {

namespace impl {

Entity& SetDrawImpl(Entity& entity, std::string_view drawable_name) {
	entity.Add<IDrawable>(drawable_name);
	return entity;
}

} // namespace impl

bool HasDraw(const Entity& entity) {
	return entity.Has<IDrawable>();
}

Entity& RemoveDraw(Entity& entity) {
	entity.Remove<IDrawable>();
	return entity;
}

Entity& SetDrawOffset(Entity& entity, const V2_float& offset) {
	entity.TryAdd<impl::Offsets>().custom.position = offset;
	return entity;
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
		entity.Add<Visible>(visible);
		entity.InvokeScript<&impl::IScript::OnShow>();
	} else {
		entity.InvokeScript<&impl::IScript::OnHide>();
		entity.Remove<Visible>();
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
		entity.GetImpl<Depth>() = depth;
	} else {
		entity.Add<Depth>(depth);
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
		entity.Add<Tint>(color);
	} else {
		entity.Remove<Tint>();
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
		std::invoke(check_vertical_flip);
		return tex_coords;
	}

	V2_int texture_size{ GetTextureSize(entity) };

	if (texture_size.IsZero()) {
		std::invoke(check_vertical_flip);
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

	std::invoke(check_vertical_flip);

	return tex_coords;
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

Entity CreateRect(
	Scene& scene, const V2_float& position, const V2_float& size, const Color& color,
	float line_width, Origin origin
) {
	return CreateRect(static_cast<Manager&>(scene), position, size, color, line_width, origin);
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

Entity CreateCircle(
	Scene& scene, const V2_float& position, float radius, const Color& color, float line_width
) {
	return CreateCircle(static_cast<Manager&>(scene), position, radius, color, line_width);
}

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

bool EntityDepthCompare::operator()(const Entity& a, const Entity& b) const {
	auto depth_a{ GetDepth(a) };
	auto depth_b{ GetDepth(b) };
	if (depth_a == depth_b) {
		return a.WasCreatedBefore(b);
	}
	return depth_a < depth_b;
}

namespace impl {

struct ShapeDrawInfo {
	ShapeDrawInfo(const Entity& entity) :
		transform{ GetDrawTransform(entity) },
		tint{ GetTint(entity) },
		depth{ GetDepth(entity) },
		line_width{ entity.GetOrDefault<LineWidth>() },
		state{ game.shader.Get<ShapeShader::Quad>(), GetBlendMode(entity),
			   entity.GetOrDefault<Camera>(), entity.GetOrDefault<PostFX>() } {}

	Transform transform;
	Color tint;
	Depth depth;
	LineWidth line_width;
	RenderState state;
};

void DrawTexture(RenderData& ctx, const Entity& entity, bool flip_texture) {
	ShapeDrawInfo info{ entity };

	Sprite sprite{ entity };
	const auto& texture{ sprite.GetTexture() };
	auto display_size{ sprite.GetSize() };
	auto texture_coordinates{ sprite.GetTextureCoordinates(flip_texture) };

	auto origin{ GetDrawOrigin(entity) };
	auto pre_fx{ entity.GetOrDefault<PreFX>() };

	ctx.AddTexturedQuad(
		texture, info.transform, display_size, origin, info.tint, info.depth, texture_coordinates,
		info.state, pre_fx
	);
}

void DrawRect(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Rect>());
	const auto& rect{ entity.Get<Rect>() };
	auto origin{ GetDrawOrigin(entity) };
	ctx.AddQuad(
		info.transform, rect.GetSize(), origin, info.tint, info.depth, info.line_width, info.state
	);
}

void DrawCapsule(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Capsule>());
	const auto& capsule{ entity.Get<Capsule>() };
	auto [start, end] = capsule.GetWorldVertices(info.transform);

	// TODO: Replace with arcs.
	ctx.AddCircle(
		Transform{ start }, capsule.radius, info.tint, info.depth, info.line_width, info.state
	);
	ctx.AddCircle(
		Transform{ end }, capsule.radius, info.tint, info.depth, info.line_width, info.state
	);

	// TODO: Replace with two lines connecting arcs.
	ctx.AddLine(start, end, info.tint, info.depth, info.line_width, info.state);
}

void DrawCircle(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Circle>());
	const auto& circle{ entity.Get<Circle>() };
	info.state.shader_pass = game.shader.Get<ShapeShader::Circle>();
	ctx.AddCircle(
		info.transform, circle.radius, info.tint, info.depth, info.line_width, info.state
	);
}

void DrawLine(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Line>());
	const auto& line{ entity.Get<Line>() };

	auto [start, end] = line.GetWorldVertices(info.transform);

	ctx.AddLine(start, end, info.tint, info.depth, info.line_width, info.state);
}

void DrawPolygon(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Polygon>());
	const auto& polygon{ entity.Get<Polygon>() };

	auto points{ polygon.GetWorldVertices(info.transform) };

	ctx.AddPolygon(points, info.tint, info.depth, info.line_width, info.state);
}

void DrawTriangle(RenderData& ctx, const Entity& entity) {
	ShapeDrawInfo info{ entity };
	PTGN_ASSERT(entity.Has<Triangle>());
	const auto& triangle{ entity.Get<Triangle>() };

	auto points{ triangle.GetWorldVertices(info.transform) };

	ctx.AddTriangle(points, info.tint, info.depth, info.line_width, info.state);
}

} // namespace impl

} // namespace ptgn