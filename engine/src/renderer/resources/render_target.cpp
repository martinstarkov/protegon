#include "renderer/resources/render_target.h"

#include <optional>

#include "core/math/vector2.h"
#include "renderer/resources/framebuffer.h"
#include "renderer/resources/renderbuffer.h"
#include "renderer/resources/texture.h"

namespace ptgn::impl {

V2_int RenderTarget::GetSize() const {
	return size_;
}

TextureFormat RenderTarget::GetFormat() const {
	return format_;
}

RenderTarget::RenderTarget(
	const Framebuffer& framebuffer, const std::optional<Texture>& color,
	const std::optional<Renderbuffer>& depth, V2_int size, TextureFormat format
) :
	framebuffer_{ framebuffer },
	color_{ color },
	depth_{ depth },
	size_{ size },
	format_{ format } {}

} // namespace ptgn::impl