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
	TextureId id{ 0 };
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
	GLenum cull_face{ GL_BACK };
	GLenum front_face{ GL_CCW };

	bool operator==(const CullState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct BlendingEnabled {
	BlendingEnabled() = default;

	BlendingEnabled(GLboolean value) : value{ value } {}

	GLboolean value{ GL_FALSE };

	operator GLboolean() const {
		return value;
	}
};

struct ActiveTextureSlot {
	ActiveTextureSlot() = default;

	ActiveTextureSlot(Id value) : value{ value } {}

	Id value{ 0 };

	operator Id() const {
		return value;
	}
};

struct ClearColor {
	ClearColor() = default;

	ClearColor(Color value) : value{ value } {}

	Color value{};

	operator Color() const {
		return value;
	}
};

struct ClearDepth {
	ClearDepth() = default;

	ClearDepth(double value) : value{ value } {}

	double value{ 0.0 };

	operator double() const {
		return value;
	}
};

struct ClearStencil {
	ClearStencil() = default;

	ClearStencil(int value) : value{ value } {}

	int value{ 0 };

	operator int() const {
		return value;
	}
};

struct PolygonModeFront {
	PolygonModeFront() = default;

	PolygonModeFront(GLenum value) : value{ value } {}

	GLenum value{ GL_FILL };

	operator GLenum() const {
		return value;
	}
};

struct PolygonModeBack {
	PolygonModeBack() = default;

	PolygonModeBack(GLenum value) : value{ value } {}

	GLenum value{ GL_FILL };

	operator GLenum() const {
		return value;
	}
};

struct LineWidth {
	LineWidth() = default;

	LineWidth(float value) : value{ value } {}

	GLfloat value{ 1.0f };

	operator GLfloat() const {
		return value;
	}
};

using StateChange = std::variant<
	FramebufferId, RenderbufferId, VertexBufferId, UniformBufferId, ShaderId, VertexArrayId,
	Viewport, DepthTestState, BlendMode, BlendingEnabled, ColorMaskState, ActiveTextureSlot,
	TextureUnits, ClearColor, ClearDepth, ClearStencil, ScissorState, PolygonModeFront,
	PolygonModeBack, LineWidth, CullState, StencilState>;

struct State {
	// Core object bindings
	FramebufferId framebuffer;
	RenderbufferId renderbuffer;
	VertexBufferId vertex_buffer;
	UniformBufferId uniform_buffer;
	ShaderId shader;
	VertexArrayId vertex_array;

	Viewport viewport;

	DepthTestState depth;

	BlendMode blend_mode{ BlendMode::ReplaceRGBA };
	BlendingEnabled blending;

	ColorMaskState color_mask;

	ActiveTextureSlot active_texture_slot;
	TextureUnits texture_units;

	ClearColor clear_color;
	ClearDepth clear_depth;
	ClearStencil clear_stencil;

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
