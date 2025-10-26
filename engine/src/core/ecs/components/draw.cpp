#include "core/ecs/components/draw.h"

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "core/app/game.h"
#include "core/app/manager.h"
#include "core/ecs/components/drawable.h"
#include "core/ecs/components/effects.h"
#include "core/ecs/components/generic.h"
#include "core/ecs/components/offsets.h"
#include "core/ecs/components/sprite.h"
#include "core/ecs/components/transform.h"
#include "core/ecs/entity.h"
#include "core/scripting/script.h"
#include "core/scripting/script_interfaces.h"
#include "core/util/concepts.h"
#include "debug/runtime/assert.h"
#include "math/geometry/arc.h"
#include "math/geometry/capsule.h"
#include "math/geometry/circle.h"
#include "math/geometry/ellipse.h"
#include "math/geometry/line.h"
#include "math/geometry/polygon.h"
#include "math/geometry/rect.h"
#include "math/geometry/rounded_rect.h"
#include "math/geometry/shape.h"
#include "math/geometry/triangle.h"
#include "math/vector2.h"
#include "math/vector4.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/api/flip.h"
#include "renderer/api/origin.h"
#include "renderer/materials/shader.h"
#include "renderer/materials/texture.h"
#include "renderer/render_data.h"
#include "renderer/renderer.h"
#include "renderer/text/text.h"
#include "world/scene/camera.h"
#include "world/scene/scene.h"

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

Entity SetVisible(Entity entity, bool visible) {
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

Entity Show(Entity entity) {
	return SetVisible(entity, true);
}

Entity Hide(Entity entity) {
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

void DrawTexture(const Entity& entity, bool flip_texture) {
	Sprite sprite{ entity };

	game.renderer.DrawTexture(
		sprite.GetTexture(), GetDrawTransform(entity), sprite.GetSize(), GetDrawOrigin(entity),
		GetTint(entity), GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
		entity.GetOrDefault<PreFX>(), entity.GetOrDefault<PostFX>(),
		sprite.GetTextureCoordinates(flip_texture)
	);
}

void DrawText(
	Text text, const V2_int& text_size, const Camera& camera, const Color& additional_tint,
	Origin offset_origin, const V2_float& offset_size
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

	Tint tint{ GetTint(text) };
	Transform transform{ GetDrawTransform(text) };
	Camera cam{ text.GetOrDefault<Camera>() };

	if (tint.a == 0 || additional_tint.a == 0) {
		return;
	}

	if (camera) {
		cam = camera;
	}

	// Offset text so it is centered on the offset origin and size.
	auto offset{ -GetOriginOffset(offset_origin, offset_size * Abs(transform.GetScale())) };
	transform.Translate(offset);

	if (bool is_hd{ text.IsHD() }) {
		auto scene_scale{ text.GetScene().GetRenderTargetScaleRelativeTo(cam) };

		PTGN_ASSERT(scene_scale.BothAboveZero());

		transform.Scale(transform.GetScale() / scene_scale);

		if (text.GetFontSize(is_hd, cam) != text.Get<impl::CachedFontSize>()) {
			text.RecreateTexture(cam);
		}
	}

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

	auto texture_coordinates{ Sprite{ text }.GetTextureCoordinates(false) };

	Color text_tint{ additional_tint.Normalized() * tint.Normalized() };

	game.renderer.DrawTexture(
		text_texture, transform, size, GetDrawOrigin(text), text_tint, GetDepth(text),
		GetBlendMode(text), cam, text.GetOrDefault<PreFX>(), text.GetOrDefault<PostFX>(),
		texture_coordinates
	);
}

void DrawText(const Entity& entity) {
	impl::DrawText(entity, V2_float{}, Camera{}, color::White, Origin::Center, V2_float{});
}

template <ShapeType T>
static void DrawShape(const Entity& entity) {
	PTGN_ASSERT(entity.Has<T>(), "Entity does not have shape: ", type_name<T>());

	Origin origin{ Origin::Center };

	if constexpr (IsAnyOf<T, Rect, RoundedRect>) {
		origin = GetDrawOrigin(entity);
	}

	const auto& shape{ entity.Get<T>() };

	game.renderer.DrawShape(
		GetDrawTransform(entity), shape, GetTint(entity), entity.GetOrDefault<LineWidth>(), origin,
		GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
		entity.GetOrDefault<PostFX>(), entity.GetOrDefault<ShaderPass>()
	);
}

void DrawRect(const Entity& entity) {
	DrawShape<Rect>(entity);
}

void DrawRoundedRect(const Entity& entity) {
	DrawShape<RoundedRect>(entity);
}

void DrawArc(const Entity& entity) {
	DrawShape<Arc>(entity);
}

void DrawCapsule(const Entity& entity) {
	DrawShape<Capsule>(entity);
}

void DrawCircle(const Entity& entity) {
	DrawShape<Circle>(entity);
}

void DrawEllipse(const Entity& entity) {
	DrawShape<Ellipse>(entity);
}

void DrawLine(const Entity& entity) {
	DrawShape<Line>(entity);
}

void DrawPolygon(const Entity& entity) {
	DrawShape<Polygon>(entity);
}

void DrawTriangle(const Entity& entity) {
	DrawShape<Triangle>(entity);
}

void DrawShader(const Entity& entity) {
	game.renderer.DrawShader(
		entity.Get<impl::ShaderPass>(), entity, true, color::Transparent, V2_int{},
		default_blend_mode, GetDepth(entity), GetBlendMode(entity), entity.GetOrDefault<Camera>(),
		default_texture_format, entity.GetOrDefault<PostFX>()
	);
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