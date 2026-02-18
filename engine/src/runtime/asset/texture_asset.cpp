#include "runtime/asset/texture_asset.h"

#include "core/math/vector2.h"
#include "renderer/resources/texture.h"
#include "runtime/ecs/entity.h"

namespace ptgn {

V2_int Texture::GetSize() const {
	return entity_.Get<impl::TextureObject>().GetSize();
}

TextureFormat Texture::GetFormat() const {
	return entity_.Get<impl::TextureObject>().GetFormat();
}

} // namespace ptgn