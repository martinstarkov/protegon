#include "gl_renderer.h"

#include "protegon/game.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"

namespace ptgn {

void GLRenderer::EnableLineSmoothing() {
	EnableBlending();
	gl::glEnable(GL_LINE_SMOOTH);
}

void GLRenderer::DisableLineSmoothing() {
	gl::glDisable(GL_LINE_SMOOTH);
}

void GLRenderer::EnableBlending() {
	DisableDepthTesting();
	gl::glEnable(GL_BLEND);
	gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GLRenderer::DisableBlending() {
	gl::glDisable(GL_BLEND);
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
	PTGN_CHECK(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	va.Bind();
	gl::glDrawElements(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()),
		index_count == 0 ? va.GetIndexBuffer().GetCount() : static_cast<std::uint32_t>(index_count),
		static_cast<gl::GLenum>(IndexBuffer::GetType()), nullptr
	);
}

void GLRenderer::DrawArrays(const VertexArray& va, std::size_t vertex_count) {
	PTGN_CHECK(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	va.Bind();
	gl::glDrawArrays(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), 0, static_cast<std::uint32_t>(vertex_count)
	);
}

void GLRenderer::SetLineWidth(float width) {
	gl::glLineWidth(width);
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