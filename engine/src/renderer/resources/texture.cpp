#include "renderer/resources/texture.h"

#include <memory>

#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/resource.h"

namespace ptgn {

namespace impl {

V2_int TextureObject::GetSize() const {
	return renderer_->gl->textures.GetTextureSize(resource_);
}

TextureFormat TextureObject::GetFormat() const {
	return renderer_->gl->textures.GetCache(resource_).GetFormat();
}

} // namespace impl

V2_int Texture::GetSize() const {
	return entity_.Get<impl::TextureObject>().GetSize();
}

TextureFormat Texture::GetFormat() const {
	return entity_.Get<impl::TextureObject>().GetFormat();
}

Texture::operator impl::TextureId() const {
	return entity_.Get<impl::TextureObject>();
}

} // namespace ptgn