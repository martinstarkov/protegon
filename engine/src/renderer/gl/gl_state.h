#pragma once

#include <vector>

#include "math/vector2.h"
#include "renderer/api/blend_mode.h"
#include "renderer/api/color.h"
#include "renderer/gl/gl.h"
#include "renderer/gl/gl_resource.h"

namespace ptgn::impl::gl {

struct Viewport {
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;
};

struct TextureUnitState {
	Handle<Texture> texture;
	GLenum min_filter{ GL_LINEAR };
	GLenum mag_filter{ GL_LINEAR };
	GLenum wrap_s{ GL_REPEAT };
	GLenum wrap_t{ GL_REPEAT };

	bool operator==(const TextureUnitState&) const = default;
};

struct StencilState {
	GLboolean enabled{ GL_FALSE };
	GLenum func{ GL_ALWAYS };
	GLint ref{ 0 };
	GLuint mask{ 0xFFFFFFFF };
	GLenum fail_op{ GL_KEEP };
	GLenum zfail_op{ GL_KEEP };
	GLenum zpass_op{ GL_KEEP };
	GLuint write_mask{ 0xFFFFFFFF };

	bool operator==(const StencilState&) const = default;
};

struct DepthState {
	GLboolean test{ GL_FALSE };
	GLboolean write{ GL_TRUE };
	GLenum func{ GL_LESS };
	GLfloat range_near{ 0.0f };
	GLfloat range_far{ 1.0f };

	bool operator==(const DepthState&) const = default;
};

struct ColorMaskState {
	GLboolean red{ GL_TRUE };
	GLboolean green{ GL_TRUE };
	GLboolean blue{ GL_TRUE };
	GLboolean alpha{ GL_TRUE };

	bool operator==(const ColorMaskState&) const = default;
};

struct ScissorState {
	GLboolean enabled{ GL_FALSE };
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const ScissorState&) const = default;
};

struct CullState {
	GLboolean enabled{ GL_FALSE };
	GLenum face{ GL_BACK };
	GLenum front{ GL_CCW };

	bool operator==(const CullState&) const = default;
};

struct State {
	// Core object bindings
	Handle<FrameBuffer> frame_buffer;
	Handle<RenderBuffer> render_buffer;
	Handle<UniformBuffer> uniform_buffer;
	Handle<Shader> shader;
	Handle<VertexArray> vertex_array;

	Viewport viewport;

	DepthState depth;

	BlendMode blend_mode{ BlendMode::ReplaceRGBA };
	GLboolean blending{ GL_FALSE };

	ColorMaskState color_mask;

	GLuint active_texture_slot{ 0 };
	std::vector<TextureUnitState> texture_units;

	Color clear_color;

	ScissorState scissor;

	// Polygon rasterization
	GLenum polygon_mode_front{ GL_FILL };
	GLenum polygon_mode_back{ GL_FILL };
	GLfloat line_width{ 1.0f };

	CullState cull;

	StencilState stencil;

	bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl