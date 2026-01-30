#pragma once

#include <variant>
#include <vector>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"
#include "renderer/backend/gl/gl.h"

namespace ptgn::impl::gl {

struct Viewport {
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;
};

struct TextureUnitState {
	GLuint id{ 0 };
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

struct DepthTestState {
	GLboolean test{ GL_FALSE };
	GLboolean write{ GL_TRUE };
	GLenum func{ GL_LESS };
	GLfloat range_near{ 0.0f };
	GLfloat range_far{ 1.0f };

	bool operator==(const DepthTestState&) const = default;
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

using TextureUnits = std::vector<TextureUnitState>;

struct FramebufferBinding {
	GLuint value{ 0 };
	bool operator==(const FramebufferBinding&) const = default;
};

struct RenderbufferBinding {
	GLuint value{ 0 };
	bool operator==(const RenderbufferBinding&) const = default;
};

struct VertexBufferBinding {
	GLuint value{ 0 };
	bool operator==(const VertexBufferBinding&) const = default;
};

struct UniformBufferBinding {
	GLuint value{ 0 };
	bool operator==(const UniformBufferBinding&) const = default;
};

struct ShaderBinding {
	GLuint value{ 0 };
	bool operator==(const ShaderBinding&) const = default;
};

struct VertexArrayBinding {
	GLuint value{ 0 };
	bool operator==(const VertexArrayBinding&) const = default;
};

struct BlendingEnabled {
	GLboolean value{ GL_FALSE };
	bool operator==(const BlendingEnabled&) const = default;
};

struct ActiveTextureSlot {
	GLuint value{ 0 };
	bool operator==(const ActiveTextureSlot&) const = default;
};

struct ClearColor {
	Color value{};
	bool operator==(const ClearColor&) const = default;
};

struct PolygonModeFront {
	GLenum value{ GL_FILL };
	bool operator==(const PolygonModeFront&) const = default;
};

struct PolygonModeBack {
	GLenum value{ GL_FILL };
	bool operator==(const PolygonModeBack&) const = default;
};

struct LineWidth {
	GLfloat value{ 1.0f };
	bool operator==(const LineWidth&) const = default;
};

using StateChange = std::variant<
	FramebufferBinding, RenderbufferBinding, VertexBufferBinding, UniformBufferBinding,
	ShaderBinding, VertexArrayBinding, Viewport, DepthTestState, BlendMode, BlendingEnabled,
	ColorMaskState, ActiveTextureSlot, TextureUnits, ClearColor, ScissorState, PolygonModeFront,
	PolygonModeBack, LineWidth, CullState, StencilState>;

struct State {
	// Core object bindings
	FramebufferBinding framebuffer;
	RenderbufferBinding renderbuffer;
	VertexBufferBinding vertex_buffer;
	UniformBufferBinding uniform_buffer;
	ShaderBinding shader;
	VertexArrayBinding vertex_array;

	Viewport viewport;

	DepthTestState depth;

	BlendMode blend_mode{ BlendMode::ReplaceRGBA };
	BlendingEnabled blending;

	ColorMaskState color_mask;

	ActiveTextureSlot active_texture_slot;
	TextureUnits texture_units;

	ClearColor clear_color;

	ScissorState scissor;

	// Polygon rasterization
	PolygonModeFront polygon_mode_front;
	PolygonModeBack polygon_mode_back;
	LineWidth line_width;

	CullState cull;

	StencilState stencil;

	bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl
