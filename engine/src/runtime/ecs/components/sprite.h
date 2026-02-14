#pragma once

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;

namespace impl {

struct Sprite {};

} // namespace impl

Entity SetTexture(Entity sprite, Handle<Asset::Texture> texture);

// @return Unscaled size of the entire texture in pixels.
[[nodiscard]] V2_int GetTextureSize(Entity sprite);

// TODO: Fix.
// @return Unscaled size of the cropped texture in pixels.
//[[nodiscard]] V2_int GetSize(Entity sprite) const;

// TODO: Fix.
// @return Scaled size of the cropped texture in pixels.
//[[nodiscard]] V2_float GetDisplaySize(Entity sprite) const;

// TODO: Fix.
// void SetDisplaySize(const V2_float& display_size);

// TODO: Fix.
//[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;

Entity CreateSprite(
	Manager& manager, Handle<Asset::Texture> texture, V2_float position = {}
	// TODO: Readd origin.
	// Origin draw_origin = Origin::Center
);

Entity CreateSprite(Scene& scene, const path& asset_path, V2_float position = {});

} // namespace ptgn