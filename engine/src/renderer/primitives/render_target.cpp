#include "renderer/primitives/render_target.h"

#include <optional>

#include "core/assert.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/renderer.h"

namespace ptgn {

namespace impl {

RenderTargetData::operator TextureId() const {
	PTGN_ASSERT(
		color_.has_value(), "Cannot convert render target with no color attachment to a texture id"
	);
	return *color_;
}

RenderTargetData::RenderTargetData(
	FramebufferId framebuffer, const std::optional<TextureId>& color,
	const std::optional<RenderbufferId>& depth, V2_int size, TextureFormat format
) :
	framebuffer_{ framebuffer },
	color_{ color },
	depth_{ depth },
	size_{ size },
	format_{ format } {}

V2_int RenderTargetObject::GetSize() const {
	return resource_.size_;
}

TextureFormat RenderTargetObject::GetFormat() const {
	return resource_.format_;
}

void RenderTargetObject::Resize(V2_int new_size) {
	PTGN_ASSERT(renderer_);
	PTGN_ASSERT(*this);
	renderer_->ResizeRenderTarget(resource_, new_size);
}

void RenderTargetObject::Clear(Color color, bool set_viewport) const {
	PTGN_ASSERT(renderer_);
	PTGN_ASSERT(*this);
	renderer_->ClearRenderTarget(resource_, color, set_viewport);
}

void RenderTargetObject::Bind() const {
	PTGN_ASSERT(renderer_);
	PTGN_ASSERT(*this);
	renderer_->BindRenderTarget(resource_);
}

RenderTargetObject::operator TextureId() const {
	return static_cast<TextureId>(static_cast<RenderTargetData>(*this));
}

} // namespace impl

} // namespace ptgn