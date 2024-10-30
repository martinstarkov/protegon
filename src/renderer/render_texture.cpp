#include "renderer/render_texture.h"

#include "core/game.h"
#include "core/window.h"
#include "math/geometry/polygon.h"
#include "math/vector2.h"
#include "renderer/frame_buffer.h"
#include "renderer/gl_renderer.h"
#include "renderer/renderer.h"
#include "renderer/shader.h"
#include "renderer/surface.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"
#include "utility/debug.h"

namespace ptgn {

RenderTexture::RenderTexture(const Shader& shader) :
	Texture{ nullptr,
			 game.window.GetSize(),
			 ImageFormat::RGB888,
			 TextureWrapping::ClampEdge,
			 TextureFilter::Nearest,
			 TextureFilter::Nearest,
			 false } {
	PTGN_ASSERT(Texture::IsValid(), "Failed to create render texture");
	frame_buffer_ = FrameBuffer{ *this, RenderBuffer{ GetSize() } };
	vertex_array_ = VertexArray{ *this };
	camera.SetToWindow(false);
	shader_ = shader;
	PTGN_ASSERT(shader_.IsValid(), "Cannot set invalid shader");
}

RenderTexture::RenderTexture(ScreenShader screen_shader) :
	RenderTexture{ game.shader.Get(screen_shader) } {}

const VertexArray& RenderTexture::GetVertexArray() const {
	return vertex_array_;
}

const Shader& RenderTexture::GetShader() const {
	return shader_;
}

void RenderTexture::Draw() {
	PTGN_ASSERT(shader_.IsValid(), "Cannot draw render texture without setting a valid shader");
	PTGN_ASSERT(
		vertex_array_.IsValid(), "Cannot draw render texture without setting a valid vertex array"
	);
	FrameBuffer::Unbind();
	V2_float size{ GetSize() };
	shader_.Bind();
	shader_.SetUniform("u_Texture", 0);
	shader_.SetUniform("u_Resolution", size);
	shader_.SetUniform("u_ViewProjection", camera.GetViewProjection());
	vertex_array_.Bind();
	Texture::Bind(0);
	bool depth_testing_enabled = GLRenderer::IsDepthTestingEnabled();
	if (depth_testing_enabled) {
		GLRenderer::DisableDepthTesting();
	}
	game.draw.VertexArray(vertex_array_);
	if (depth_testing_enabled) {
		GLRenderer::EnableDepthTesting();
	}
}

void RenderTexture::Bind() const {
	frame_buffer_.Bind();
}

Color RenderTexture::GetClearColor() const {
	return clear_color_;
}

void RenderTexture::SetClearColor(const Color& clear_color) {
	clear_color_ = clear_color;
}

} // namespace ptgn