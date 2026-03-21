#include "renderer/primitives/texture.h"

#include <ostream>

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
	return entity_.Get<impl::TextureObject>().GetSize();
}

TextureFormat Texture::GetFormat() const {
	return entity_.Get<impl::TextureObject>().GetFormat();
}

Texture::operator impl::TextureId() const {
	return entity_.Get<impl::TextureObject>();
}

std::ostream& operator<<(std::ostream& o, const Texture& t) {
	o << "{";
	o << "texture id: " << static_cast<impl::TextureId>(t);
	o << ", size: " << t.GetSize();
	// o << ", format: " << static_cast<std::uint32_t>(t.GetFormat());
	o << "}";
	return o;
}

} // namespace ptgn