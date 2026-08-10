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

namespace {

V2_float ToFloat(V2_int value) {
	return {
		static_cast<float>(value.x),
		static_cast<float>(value.y),
	};
}

V2_float Round(V2_float value) {
	return {
		std::round(value.x),
		std::round(value.y),
	};
}

std::optional<std::size_t> DetectSliceCountFromPath(const path& texture_path) {
	constexpr std::string_view kSliceMarker{ "_slices" };

	const std::string stem_string{ texture_path.stem().string() };
	const std::string_view stem{ stem_string };

	const auto marker{ stem.rfind(kSliceMarker) };

	if (marker == std::string_view::npos) {
		return std::nullopt;
	}

	const std::string_view digits{ stem.substr(marker + kSliceMarker.size()) };

	if (digits.empty()) {
		return std::nullopt;
	}

	std::size_t slice_count{ 0 };

	const auto [end, error]{
		std::from_chars(digits.data(), digits.data() + digits.size(), slice_count)
	};

	if (error != std::errc{} || end != digits.data() + digits.size() || slice_count == 0) {
		return std::nullopt;
	}

	return slice_count;
}

} // namespace

namespace impl {

std::optional<std::size_t> DetectSpriteStackSliceCount(
	AssetManager& assets, const TextureKey& texture_key
) {
	// Prefer the catalog path. This also works when the asset is known to the project but is not
	// currently resident in memory.
	if (auto asset{ assets.GetCatalogAsset(texture_key) }) {
		if (auto slice_count{ DetectSliceCountFromPath(asset->source_path) }) {
			return slice_count;
		}
	}

	// Runtime/manual loads are not necessarily cataloged. A loaded asset still stores the source
	// path directly on its asset entity.
	AssetAccessor accessor{ assets };

	if (!accessor.Has<Texture>(texture_key)) {
		return std::nullopt;
	}

	Texture texture{ accessor.Get<Texture>(texture_key) };

	if (auto asset_path{ texture.GetEntity().TryGet<AssetPath>() }) {
		return DetectSliceCountFromPath(asset_path->value);
	}

	return std::nullopt;
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
	const auto texture_size{ GetTextureSize(entity) };

	if (!texture_size) {
		return std::nullopt;
	}

	const std::size_t slice_count{ GetSpriteStackSliceCount(entity) };

	if (slice_count == 0 || slice_count > static_cast<std::size_t>(texture_size->y)) {
		return std::nullopt;
	}

	const int slice_count_int{ static_cast<int>(slice_count) };

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

	const auto texture_size{ GetTextureSize(entity) };

	if (!texture_size) {
		PTGN_WARN("Sprite stack texture does not have a valid texture size");
		return;
	}

	const std::size_t slice_count{ impl::GetSpriteStackSliceCount(entity) };

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

	const int slice_count_int{ static_cast<int>(slice_count) };

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

	const V2_int slice_size{
		texture_size->x,
		texture_size->y / slice_count_int,
	};

	Transform base_transform{ GetDrawTransform(entity) };
	const V2_float absolute_scale{ Abs(base_transform.scale) };

	PTGN_ASSERT(!absolute_scale.HasZero(), "Sprite stack scale cannot have a zero component");

	const V2_float display_size{ ToFloat(slice_size) * absolute_scale };

	// display_size already includes the magnitude of the entity scale.
	// Keep only the signs in the transform so negative scale still flips the stack.
	base_transform.Scale(1.0f / absolute_scale);

	const auto depth{ GetDepth(entity) };
	const auto origin{ entity.GetOrDefault<Origin>() };
	const auto tint{ GetTint(entity) };
	const auto effects{ impl::GetEffectParams(entity) };
	const auto entity_id{ entity.Get<UUID>() };

	ctx.SetBlendMode(GetBlendMode(entity));

	for (int draw_layer{ 0 }; draw_layer < slice_count_int; ++draw_layer) {
		// Draw bottom -> top. slice_order only determines where that layer is stored in the source
		// vertical strip.
		const int source_layer{
			data.slice_order == SpriteStackSliceOrder::BottomToTop
				? draw_layer
				: slice_count_int - draw_layer - 1
		};

		const V2_int source_position{
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