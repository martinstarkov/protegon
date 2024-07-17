#include "gl_renderer.h"

#include "protegon/game.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_helper.h"

namespace ptgn {

void GLRenderer::Init() {
	gl::glClearDepth(1.0); /* Enables Clearing Of The Depth Buffer */
	gl::glEnable(GL_DEPTH_TEST);
	gl::glDepthFunc(GL_LESS);
	gl::glEnable(GL_LINE_SMOOTH);
	gl::glShadeModel(GL_SMOOTH);
	gl::glEnable(GL_BLEND);
	gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void GLRenderer::DrawIndexed(const VertexArray& va, std::int32_t index_count) {
	PTGN_CHECK(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	va.Bind();
	gl::glDrawElements(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()),
		index_count == 0 ? va.GetIndexBuffer().GetCount() : index_count,
		static_cast<gl::GLenum>(impl::IndexBufferInstance::GetType()), nullptr
	);
}

void GLRenderer::DrawLines(const VertexArray& va, std::uint32_t vertex_count) {
	va.Bind();
	gl::glDrawArrays(static_cast<gl::GLenum>(PrimitiveMode::Lines), 0, vertex_count);
}

void GLRenderer::SetLineWidth(float width) {
	gl::glLineWidth(width);
}

void GLRenderer::SetClearColor(const Color& color) {
	auto c = color.Normalized();
	gl::glClearColor(c[0], c[1], c[2], c[3]);
}

void GLRenderer::SetSize(const V2_int& size) {
	gl::glViewport(0, 0, size.x, size.y);
}

void GLRenderer::Clear() {
	gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

} // namespace ptgn