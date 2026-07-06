#include "renderer/resources/framebuffer.h"

#include "core/assert.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/renderer.h"
#include "renderer/resources/id.h"
#include "renderer/resources/resource.h"
#include "renderer/resources/texture.h"

namespace ptgn {

namespace impl {

void FramebufferObject::Resize(V2_int new_size) {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->Resize(*this, new_size);
}

void FramebufferObject::Bind() {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->SetFramebuffer(this);
}

void FramebufferObject::Clear(Color clear_color, bool restore_bind) {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->Clear(*this, clear_color, restore_bind);
}

void FramebufferObject::Clear(Depth clear_depth, bool restore_bind) {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->Clear(*this, clear_depth, restore_bind);
}

void FramebufferObject::Clear(Stencil clear_stencil, bool restore_bind) {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->Clear(*this, clear_stencil, restore_bind);
}

void FramebufferObject::Clear(DepthStencil clear_depth_stencil, bool restore_bind) {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	renderer->Clear(*this, clear_depth_stencil, restore_bind);
}

TextureId FramebufferObject::GetTexture() const {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	return renderer->GetTexture(*this);
}

TextureDesc FramebufferObject::GetDesc() const {
	PTGN_ASSERT(*this, "Framebuffer must be valid");
	return renderer->GetDesc(*this).value();
}

} // namespace impl

} // namespace ptgn