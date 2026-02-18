#include "renderer/resources/texture.h"

#include "core/math/vector2.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/resources/resource.h"

namespace ptgn::impl {

V2_int TextureObject::GetSize() const {
	return renderer_->gl->textures.GetTextureSize(resource_);
}

TextureFormat TextureObject::GetFormat() const {
	return renderer_->gl->textures.GetCache(resource_).GetFormat();
}

template class Resource<Texture>;

} // namespace ptgn::impl