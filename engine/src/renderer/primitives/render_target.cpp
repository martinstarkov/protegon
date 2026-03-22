#include "renderer/primitives/render_target.h"

#include "core/assert.h"
#include "core/math/vector2.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/id.h"
#include "renderer/primitives/resource.h"
#include "renderer/primitives/texture_format.h"
#include "renderer/renderer.h"

namespace ptgn::impl {

void RenderTargetObject::Resize(V2_int new_size) {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->ResizeRenderTarget(resource_, new_size);
}

void RenderTargetObject::Clear(Color color, bool set_viewport) const {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->ClearRenderTarget(resource_, color, set_viewport);
}

impl::TextureId RenderTargetObject::GetTextureId() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetTexture(resource_);
}

void RenderTargetObject::Bind() const {
	PTGN_ASSERT(renderer_ && *this);
	renderer_->BindRenderTarget(resource_);
}

V2_int RenderTargetObject::GetSize() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetSize(resource_);
}

TextureFormat RenderTargetObject::GetFormat() const {
	PTGN_ASSERT(renderer_ && *this);
	return renderer_->GetRenderTargetTextureFormat(resource_);
}

} // namespace ptgn::impl