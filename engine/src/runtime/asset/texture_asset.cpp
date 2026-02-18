#include "runtime/asset/texture_asset.h"

#include "app/context.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_handle.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"

namespace ptgn {

V2_int Texture::GetSize() const {}

TextureFormat Texture::GetFormat() const {}

void Texture::Destroy() {
	/*auto& texture  = entity_.Get<impl::Texture>();
	auto& renderer = entity_.GetScene().app().renderer;

	renderer.DestroyTexture(texture);
	entity_.Destroy();*/
}

template class impl::RefCountedAsset<Texture>;

} // namespace ptgn