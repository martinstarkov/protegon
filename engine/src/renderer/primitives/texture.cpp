#include "renderer/primitives/texture.h"

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

V2_int TextureObject::GetSize() const {
	return renderer_->GetTextureSize(resource_);
}

TextureFormat TextureObject::GetFormat() const {
	return renderer_->GetTextureFormat(resource_);
}

} // namespace impl

V2_int Texture::GetSize() const {
	return GetEntity().Get<impl::TextureObject>().GetSize();
}

TextureFormat Texture::GetFormat() const {
	return GetEntity().Get<impl::TextureObject>().GetFormat();
}

Texture::operator impl::TextureId() const {
	return GetEntity().Get<impl::TextureObject>();
}

} // namespace ptgn