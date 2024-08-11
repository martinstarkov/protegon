#include "gl_renderer.h"

#include "protegon/game.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"

namespace ptgn {

void GLRenderer::EnableLineSmoothing() {
	gl::glEnable(GL_BLEND);
	gl::glEnable(GL_LINE_SMOOTH);
}

void GLRenderer::DisableLineSmoothing() {
	gl::glDisable(GL_LINE_SMOOTH);
}

void GLRenderer::SetBlendMode(BlendMode mode /* = BlendMode::Blend*/) {
	if (mode == BlendMode::None) {
		gl::glDisable(GL_BLEND);
		gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	} else {
		DisableDepthTesting();
		gl::glEnable(GL_BLEND);
		gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		switch (mode) {
			case BlendMode::Blend:	  gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;
			case BlendMode::Add:	  gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;
			case BlendMode::Modulate: gl::glBlendFunc(GL_ZERO, GL_SRC_COLOR); break;
			case BlendMode::Multiply:
				// TODO: Check that this works correctly.
				gl::glBlendFunc(GL_DST_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				break;
			default: PTGN_ERROR("Failed to identify blend mode");
		}
	}
}

void GLRenderer::EnableDepthTesting() {
	gl::glClearDepth(1.0); /* Enables Clearing Of The Depth Buffer */
	gl::glEnable(GL_DEPTH_TEST);
	gl::glDepthFunc(GL_LESS);
}

void GLRenderer::DisableDepthTesting() {
	gl::glDisable(GL_DEPTH_TEST);
}

void GLRenderer::DrawElements(const VertexArray& va, std::size_t index_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	va.Bind();
	gl::glDrawElements(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()),
		index_count == 0 ? va.GetIndexBuffer().GetCount() : static_cast<std::uint32_t>(index_count),
		static_cast<gl::GLenum>(IndexBuffer::GetType()), nullptr
	);
}

void GLRenderer::DrawArrays(const VertexArray& va, std::size_t vertex_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	va.Bind();
	gl::glDrawArrays(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), 0, static_cast<std::uint32_t>(vertex_count)
	);
}

std::int32_t GLRenderer::GetMaxTextureSlots() {
	std::int32_t max_texture_slots{ 0 };
	gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
	return max_texture_slots;
}

void GLRenderer::SetClearColor(const Color& color) {
	auto c = color.Normalized();
	gl::glClearColor(c[0], c[1], c[2], c[3]);
}

void GLRenderer::SetViewport(const V2_int& position, const V2_int& size) {
	gl::glViewport(position.x, position.y, size.x, size.y);
	// PTGN_LOG("Setting OpenGL Viewport to ", size);
}

void GLRenderer::Clear() {
	gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

} // namespace ptgn