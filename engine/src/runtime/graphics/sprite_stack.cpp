#include "runtime/graphics/sprite_stack.h"

#include <charconv>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/draw_context.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/effect_params.h"
#include "renderer/resources/id.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/ecs/tag.h"
#include "runtime/ecs/uuid.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"

namespace ptgn {

namespace impl {

std::optional<std::size_t> DetectSpriteStackSliceCount(
	AssetManager& assets,
	const TextureKey& texture_key
) {
	return DetectTexturePathCount(assets, texture_key, "_slices");
}

std::size_t GetSpriteStackSliceCount(Entity entity) {
	PTGN_ASSERT(entity, "Cannot get slice count from an invalid entity");

	const auto data{ entity.TryGet<SpriteStackData>() };

	if (!data) {
		return 0;
	}

	if (const auto texture_key{ entity.TryGet<TextureKey>() }) {
		auto& assets{ entity.GetScene().ctx().asset };

		if (auto detected{ DetectSpriteStackSliceCount(assets, *texture_key) }) {
			return *detected;
		}
	}

	return data->slice_count;
}

std::optional<V2_int> GetSpriteStackSliceSize(Entity entity) {
	auto texture_size{ GetTextureSize(entity) };

	if (!texture_size) {
		return std::nullopt;
	}

	std::size_t slice_count{ GetSpriteStackSliceCount(entity) };

	if (slice_count == 0 || slice_count > static_cast<std::size_t>(texture_size->y)) {
		return std::nullopt;
	}

	int slice_count_int{ static_cast<int>(slice_count) };

	if (texture_size->y % slice_count_int != 0) {
		return std::nullopt;
	}

	return V2_int{
		texture_size->x,
		texture_size->y / slice_count_int,
	};
}

} // namespace impl

SpriteStack::SpriteStack(Entity entity) : Entity{ entity } {}

void SpriteStack::Draw(DrawContext& ctx, Entity entity) {
	if (!entity.Has<SpriteStackData>()) {
		PTGN_WARN("Sprite stack cannot be drawn without SpriteStackData component");
		return;
	}

	const auto& data{ entity.Get<SpriteStackData>() };

	auto texture{ impl::GetTexture(entity) };

	if (!texture) {
		PTGN_WARN("Sprite stack does not have a valid texture or texture key");
		return;
	}

	auto texture_size{ GetTextureSize(entity) };

	if (!texture_size) {
		PTGN_WARN("Sprite stack texture does not have a valid texture size");
		return;
	}

	std::size_t slice_count{ impl::GetSpriteStackSliceCount(entity) };

	if (slice_count == 0) {
		PTGN_WARN(
			"Sprite stack slice count must be positive. Set SpriteStackData::slice_count or use a "
			"texture file name ending in _slicesN"
		);
		return;
	}

	if (slice_count > static_cast<std::size_t>(texture_size->y)) {
		PTGN_WARN(
			"Sprite stack slice count ",
			slice_count,
			" exceeds texture height ",
			texture_size->y
		);
		return;
	}

	auto slice_count_int{ static_cast<int>(slice_count) };

	if (texture_size->y % slice_count_int != 0) {
		PTGN_WARN(
			"Sprite stack texture height ",
			texture_size->y,
			" is not evenly divisible by ",
			slice_count,
			" slices"
		);
		return;
	}

	V2_int slice_size{
		texture_size->x,
		texture_size->y / slice_count_int,
	};

	Transform base_transform{ GetDrawTransform(entity) };
	V2_float absolute_scale{ Abs(base_transform.scale) };

	PTGN_ASSERT(!absolute_scale.HasZero(), "Sprite stack scale cannot have a zero component");

	V2_float display_size{ slice_size * absolute_scale };

	// display_size already includes the magnitude of the entity scale.
	// Keep only the signs in the transform so negative scale still flips the stack.
	base_transform.Scale(1.0f / absolute_scale);

	auto depth{ GetDepth(entity) };
	auto origin{ entity.GetOrDefault<Origin>() };
	auto tint{ GetTint(entity) };
	auto effects{ impl::GetEffectParams(entity) };
	auto entity_id{ entity.Get<UUID>() };

	ctx.SetBlendMode(GetBlendMode(entity));

	for (int draw_layer{ 0 }; draw_layer < slice_count_int; ++draw_layer) {
		// Draw bottom -> top. slice_order only determines where that layer is stored in the source
		// vertical strip.
		int source_layer{
			data.slice_order == SpriteStackSliceOrder::BottomToTop
				? draw_layer
				: slice_count_int - draw_layer - 1
		};

		V2_int source_position{
			0,
			source_layer * slice_size.y,
		};

		Transform layer_transform{ base_transform };

		// Apply this after resolving the world transform so height remains screen/world aligned
		// instead of rotating with the car.
		layer_transform.position += data.layer_offset * static_cast<float>(draw_layer);

		if (data.pixel_snap) {
			layer_transform.position = Round(layer_transform.position);
		}

		TextureDrawParams params{
			.depth{ depth },
			.size{ display_size },
			.origin = origin,
			.tint{ tint },
			.texture_coordinates{ impl::GetTextureCoordinates(
				source_position,
				slice_size,
				*texture_size,
				false,
				true
			) },
			.effects{ effects },
			.entity_id = entity_id,
		};

		ctx.DrawTexture(layer_transform, texture, std::move(params));
	}
}

SpriteStack& SpriteStack::SetTexture(TextureKey texture_key) {
	Add<TextureKey>(std::move(texture_key));
	return *this;
}

SpriteStack CreateSpriteStack(
	Scene& scene, Transform transform, TextureKey texture_key, SpriteStackData data, Origin origin
) {
	SpriteStack stack{ scene.CreateEntity() };

	stack.Add<Tag>("Sprite Stack");
	stack.Add<Visible>(true);
	stack.Add<TextureKey>(std::move(texture_key));
	stack.Add<SpriteStackData>(std::move(data));
	stack.Add<Transform>(transform);
	stack.Add<Origin>(origin);

	SetDraw<SpriteStack>(stack);

	return stack;
}

} // namespace ptgn