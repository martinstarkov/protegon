#include "gl_renderer.h"

#include "protegon/game.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"

namespace ptgn {

void GLRenderer::EnableLineSmoothing() {
	gl::glEnable(GL_BLEND);
#ifndef __EMSCRIPTEN__
	gl::glEnable(GL_LINE_SMOOTH);
#endif
}

void GLRenderer::DisableLineSmoothing() {
#ifndef __EMSCRIPTEN__
	gl::glDisable(GL_LINE_SMOOTH);
#endif
}

void GLRenderer::SetBlendMode(BlendMode mode /* = BlendMode::Blend*/) {
	if (mode == BlendMode::None) {
		gl::glDisable(GL_BLEND);
		// gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
	} else {
		DisableDepthTesting();
		gl::glEnable(GL_BLEND);
		// gl::glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

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
#ifdef __EMSCRIPTEN__
	gl::glClearDepthf(1.0);
#else
	gl::glClearDepth(1.0); /* Enables Clearing Of The Depth Buffer */
#endif
	gl::glEnable(GL_DEPTH_TEST);
	gl::glDepthFunc(GL_LESS);
}

void GLRenderer::DisableDepthTesting() {
	gl::glDisable(GL_DEPTH_TEST);
}

void GLRenderer::DrawElements(const VertexArray& va, std::size_t index_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		va.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	PTGN_ASSERT(
		va.HasIndexBuffer(), "Cannot draw vertex array with uninitialized or destroyed index buffer"
	);
	va.Bind();
	PTGN_ASSERT(
		VertexArray::BoundId() == static_cast<std::int32_t>(va.GetInstance()->id_),
		"Failed to bind vertex array id"
	);
	gl::glDrawElements(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), static_cast<std::uint32_t>(index_count),
		static_cast<gl::GLenum>(impl::GetType<std::uint32_t>()), nullptr
	);
}

void GLRenderer::DrawArrays(const VertexArray& va, std::size_t vertex_count) {
	PTGN_ASSERT(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");
	PTGN_ASSERT(
		va.HasVertexBuffer(),
		"Cannot draw vertex array with uninitialized or destroyed vertex buffer"
	);
	va.Bind();
	gl::glDrawArrays(
		static_cast<gl::GLenum>(va.GetPrimitiveMode()), 0, static_cast<std::uint32_t>(vertex_count)
	);
}

std::int32_t GLRenderer::GetMaxTextureSlots() {
	std::int32_t max_texture_slots{ -1 };
	gl::glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &max_texture_slots);
	PTGN_ASSERT(max_texture_slots >= 0, "Failed to retrieve device maximum texture slots");
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

void GLRenderer::SetPolygonMode(PolygonMode mode) {
	gl::glPolygonMode(GL_FRONT_AND_BACK, static_cast<gl::GLenum>(mode));
}

} // namespace ptgn