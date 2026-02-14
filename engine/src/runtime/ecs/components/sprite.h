#pragma once

#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "renderer/primitives/drawable.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

class Scene;
class Renderer;

namespace impl {

struct Sprite {
	static void Draw(Renderer& renderer, Entity entity);
};

} // namespace impl

Entity SetTexture(Entity sprite, Handle<Asset::Texture> texture);

Entity CreateSprite(
	Scene& scene, Handle<Asset::Texture> texture, V2_float position = {},
	Origin draw_origin = Origin::Center
);

Entity CreateSprite(
	Scene& scene, const path& asset_path, V2_float position = {},
	Origin draw_origin = Origin::Center
);

PTGN_REGISTER_DRAWABLE(impl::Sprite);

} // namespace ptgn