#include "renderer/pipeline/render_target_pool.h"

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture_format.h"

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