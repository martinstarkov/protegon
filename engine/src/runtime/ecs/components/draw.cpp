#include "runtime/ecs/components/draw.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string_view>
#include <vector>

#include "app/context.h"
#include "core/assert.h"
#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/graphics/flip.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/handle.h"
#include "renderer/resources/texture.h"
#include "renderer/resources/vertex.h"
#include "runtime/ecs/components/drawable.h"
#include "runtime/ecs/components/generic.h"
#include "runtime/ecs/components/sprite.h"
#include "runtime/ecs/components/transform_component.h"
#include "runtime/ecs/entity.h"
#include "runtime/event/event_handler.h"
#include "runtime/scene/scene.h"

namespace ptgn {

namespace impl {

Entity SetDraw(Entity entity, std::string_view drawable_name) {
	entity.Add<IDrawable>(drawable_name);
	return entity;
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

} // namespace impl

bool HasDraw(Entity entity) {
	return entity.Has<impl::IDrawable>();
}

Entity RemoveDraw(Entity entity) {
	entity.Remove<impl::IDrawable>();
	return entity;
}

void SortByDepth(std::vector<Entity>& entities, bool ascending) {
	std::ranges::sort(entities, impl::EntityDepthCompare{ ascending });
}

Entity SetDrawOrigin(Entity entity, Origin origin) {
	entity.Add<Origin>(origin);
	return entity;
}

Origin GetDrawOrigin(Entity entity) {
	return entity.GetOrDefault<Origin>(Origin::Center);
}

Entity SetVisible(Entity entity, bool visible) {
	if (visible) {
		if (entity.Has<impl::Visible>()) {
			return entity;
		}
		entity.Add<impl::Visible>();
		EntityShow show;
		if (entity.HasScene()) {
			entity.GetScene().app().events.Emit(show);
		}
	} else {
		if (!entity.Has<impl::Visible>()) {
			return entity;
		}
		entity.Remove<impl::Visible>();
		EntityHide hide;
		if (entity.HasScene()) {
			entity.GetScene().app().events.Emit(hide);
		}
	}
	return entity;
}

Entity Show(Entity entity) {
	return SetVisible(entity, true);
}

Entity Hide(Entity entity) {
	return SetVisible(entity, false);
}

bool IsVisible(Entity entity) {
	return entity.Has<impl::Visible>();
}

Entity SetDepth(Entity entity, Depth depth) {
	entity.Add<Depth>(depth);
	return entity;
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

Entity SetBlendMode(Entity entity, BlendMode blend_mode) {
	entity.Add<BlendMode>(blend_mode);
	return entity;
}

BlendMode GetBlendMode(Entity entity) {
	return entity.GetOrDefault<BlendMode>(BlendMode::Blend);
}

Entity SetTint(Entity entity, Color color) {
	if (color != impl::Tint{}) {
		entity.Add<impl::Tint>(color);
	} else {
		entity.Remove<impl::Tint>();
	}
	return entity;
}

Color GetTint(Entity entity) {
	return entity.GetOrDefault<impl::Tint>();
}

V2_int GetTextureSize(Entity entity) {
	std::optional<V2_int> size;

	if (entity.Has<impl::TextureSize>()) {
		size = V2_int{ entity.Get<impl::TextureSize>() };
	} else if (entity.Has<Texture>()) {
		const auto& renderer{ entity.GetScene().app().renderer };
		size = renderer.GetTextureSize(entity.Get<Texture>());
	}

	PTGN_ASSERT(size.has_value(), "Entity does not have a texture");
	PTGN_ASSERT(!(*size).IsZero(), "Texture does not have a valid size");

	return *size;
}

V2_int GetCroppedSize(Entity entity) {
	if (entity.Has<impl::TextureCrop>()) {
		const auto& crop{ entity.Get<impl::TextureCrop>() };
		return crop.size;
	}
	return GetTextureSize(entity);
}

void SetDisplaySize(Entity entity, V2_float display_size) {
	entity.Add<impl::TextureSize>(display_size);
}

V2_float GetDisplaySize(Entity entity) {
	PTGN_ASSERT(entity.Has<Texture>());

	return GetCroppedSize(entity) * GetScale(entity);
}

std::array<V2_float, 4> GetTextureCoordinates(Entity entity, bool flip_vertically) {
	auto tex_coords{ impl::GetDefaultTextureCoordinates() };

	auto check_vertical_flip = [flip_vertically, &tex_coords]() {
		if (flip_vertically) {
			impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
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

	if (entity.Has<impl::TextureCrop>()) {
		const auto& crop{ entity.Get<impl::TextureCrop>() };
		if (crop != impl::TextureCrop{}) {
			tex_coords = impl::GetTextureCoordinates(crop.position, crop.size, texture_size);
		}
	}

	auto scale{ GetScale(entity) };

	bool flip_x{ scale.x < 0.0f };
	bool flip_y{ scale.y < 0.0f };

	if (flip_x && flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Both);
	} else if (flip_x) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Horizontal);
	} else if (flip_y) {
		impl::FlipTextureCoordinates(tex_coords, Flip::Vertical);
	}

	// TODO: Consider if this is necessary given entity scale already flips a texture.
	if (entity.Has<Flip>()) {
		impl::FlipTextureCoordinates(tex_coords, entity.Get<Flip>());
	}

	check_vertical_flip();

	return tex_coords;
}

Depth Depth::RelativeTo(Depth parent) const {
	parent.value_ += *this;
	return parent;
}

} // namespace ptgn