#include "renderer/stencil_mask.h"

#include "renderer/gl/gl_helper.h"
#include "renderer/gl/gl_loader.h"

namespace ptgn {

void StencilMask::Enable() {
	GLCall(glEnable(GL_STENCIL_TEST));

	GLCall(glStencilMask(0xFF));

	// Clear stencil buffer
	GLCall(glClear(GL_STENCIL_BUFFER_BIT));

	// Don't draw to color buffer (only writing to stencil).
	GLCall(glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE));

	// Always pass, and write 1 to stencil where we draw.
	GLCall(glStencilFunc(GL_ALWAYS, 1, 0xFF));
	GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE));
}

void StencilMask::DrawInside() {
	GLCall(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
	GLCall(glStencilMask(0x00));

	// Only allow drawing where stencil value == 1.
	GLCall(glStencilFunc(GL_EQUAL, 1, 0xFF));
	GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
}

void StencilMask::DrawOutside() {
	GLCall(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
	GLCall(glStencilMask(0x00));

	// Only allow drawing where stencil value != 1.
	GLCall(glStencilFunc(GL_NOTEQUAL, 1, 0xFF));
	GLCall(glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP));
}

void StencilMask::Disable() {
	GLCall(glStencilMask(0xFF)); // Reset to default.
	GLCall(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));

	// Disable stencil testing.
	GLCall(glDisable(GL_STENCIL_TEST));
}

} // namespace ptgn