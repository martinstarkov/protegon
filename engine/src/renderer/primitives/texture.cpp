#include "renderer/primitives/texture.h"

#include <memory>
#include <ostream>
#include <utility>

#include "core/log.h"
#include "core/math/vector2.h"
#include "core/util/entity_handle.h"
#include "ecs/ecs.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/backend/gl/gl_renderer.h"
#include "renderer/backend/gl/gl_texture.h"
#include "renderer/primitives/resource.h"

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

std::ostream& operator<<(std::ostream& os, TextureFormat fmt) {
	switch (fmt) {
		using enum TextureFormat;
		case R8:				return os << "R8";
		case RG8:				return os << "RG8";
		case RGB8:				return os << "RGB8";
		case RGBA8:				return os << "RGBA8";
		case R16F:				return os << "R16F";
		case RG16F:				return os << "RG16F";
		case RGB16F:			return os << "RGB16F";
		case RGBA16F:			return os << "RGBA16F";
		case R32F:				return os << "R32F";
		case RG32F:				return os << "RG32F";
		case RGB32F:			return os << "RGB32F";
		case RGBA32F:			return os << "RGBA32F";
		case Depth16:			return os << "Depth16";
		case Depth24:			return os << "Depth24";
		case Depth32F:			return os << "Depth32F";
		case Depth24_Stencil8:	return os << "Depth24_Stencil8";
		case Depth32F_Stencil8: return os << "Depth32F_Stencil8";
		case Stencil8:			return os << "Stencil8";
		case SRGB8:				return os << "SRGB8";
		case SRGB8_ALPHA8:		return os << "SRGB8_ALPHA8";
		default:				PTGN_ERROR("Unknown texture format: ", std::to_underlying(fmt));
	}
}

std::ostream& operator<<(std::ostream& os, TextureMinFilter filter) {
	switch (filter) {
		using enum TextureMinFilter;
		case Nearest:			   return os << "Nearest";
		case Linear:			   return os << "Linear";
		case NearestMipmapNearest: return os << "NearestMipmapNearest";
		case LinearMipmapNearest:  return os << "LinearMipmapNearest";
		case NearestMipmapLinear:  return os << "NearestMipmapLinear";
		case LinearMipmapLinear:   return os << "LinearMipmapLinear";
		default:				   PTGN_ERROR("Unknown texture min filter: ", std::to_underlying(filter));
	}
}

std::ostream& operator<<(std::ostream& os, TextureMagFilter filter) {
	switch (filter) {
		using enum TextureMagFilter;
		case Nearest: return os << "Nearest";
		case Linear:  return os << "Linear";
		default:	  PTGN_ERROR("Unknown texture mag filter: ", std::to_underlying(filter));
	}
}

std::ostream& operator<<(std::ostream& os, TextureWrap wrap) {
	switch (wrap) {
		using enum TextureWrap;
		case ClampToEdge:	 return os << "ClampToEdge";
		case MirroredRepeat: return os << "MirroredRepeat";
		case Repeat:		 return os << "Repeat";
		default:			 PTGN_ERROR("Unknown texture wrap: ", std::to_underlying(wrap));
	}
}

} // namespace ptgn