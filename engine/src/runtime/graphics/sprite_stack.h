#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "core/math/geometry/origin.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/entity.h"
#include "runtime/graphics/drawable.h"
#include "serialization/serialize.h"

namespace ptgn {

class AssetManager;
class DrawContext;
class Scene;

enum class SpriteStackSliceOrder : std::uint8_t {
	BottomToTop,
	TopToBottom,
};
PTGN_REFLECT_ENUM(SpriteStackSliceOrder);

/// @brief Configuration for a vertically packed sprite stack.
///
/// The source texture contains equally sized horizontal voxel slices stacked vertically.
///
/// The slice width is the full texture width.
/// The slice height is texture height / slice count.
///
/// If the source texture file name ends in "_slicesN", where N is a positive integer, that value
/// overrides slice_count. For example:
///
/// assets/car_slices8.png -> 8 slices.
struct SpriteStackData {
	/// @brief Fallback slice count when it cannot be detected from the texture asset path.
	std::size_t slice_count{ 1 };

	/// @brief Order in which slices are stored vertically in the source texture.
	SpriteStackSliceOrder slice_order{ SpriteStackSliceOrder::TopToBottom };

	/// @brief World space offset applied for each additional height slice.
	/// Negative Y moves higher slices upward on screen.
	V2_float layer_offset{ 0.0f, -1.0f };

	/// @brief Rounds each slice center to whole world pixels.
	bool pixel_snap{ true };

	PTGN_REFLECT(SpriteStackData, slice_count, slice_order, layer_offset, pixel_snap)
};

namespace impl {

/// @brief Detects the slice count from the source file path associated with a texture asset.
///
/// The TextureKey itself is never inspected. For example, a key of "car" whose source path is
/// "assets/car_slices8.png" returns 8.
std::optional<std::size_t> DetectSpriteStackSliceCount(
	AssetManager& assets, const TextureKey& texture_key
);

/// @brief Returns the effective slice count for a sprite stack.
///
/// A count encoded in the texture asset's source file path takes priority over
/// SpriteStackData::slice_count.
std::size_t GetSpriteStackSliceCount(Entity entity);

/// @brief Returns the unscaled size of one source slice.
///
/// Returns nullopt if the texture is unavailable, the slice count is invalid, or the texture height
/// is not evenly divisible by the slice count.
std::optional<V2_int> GetSpriteStackSliceSize(Entity entity);

} // namespace impl

class SpriteStack : public Entity {
public:
	SpriteStack() = default;
	explicit SpriteStack(Entity entity);

	static void Draw(DrawContext& ctx, Entity entity);

	SpriteStack& SetTexture(TextureKey texture_key);
};

SpriteStack CreateSpriteStack(
	Scene& scene, Transform transform = {}, TextureKey texture_key = {}, SpriteStackData data = {},
	Origin origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(SpriteStack);

} // namespace ptgn