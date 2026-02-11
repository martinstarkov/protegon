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
	std::uint32_t min_filter{ GL_LINEAR };
	std::uint32_t mag_filter{ GL_LINEAR };
	std::uint32_t wrap_s{ GL_REPEAT };
	std::uint32_t wrap_t{ GL_REPEAT };

	bool operator==(const TextureUnitState&) const = default;
};

struct StencilState {
	bool enabled{ false };
	std::uint32_t func{ GL_ALWAYS };
	int ref{ 0 };
	std::uint32_t mask{ 0xFFFFFFFF };
	std::uint32_t fail_op{ GL_KEEP };
	std::uint32_t zfail_op{ GL_KEEP };
	std::uint32_t zpass_op{ GL_KEEP };
	std::uint32_t write_mask{ 0xFFFFFFFF };

	bool operator==(const StencilState&) const = default;
};

struct DepthState {
	bool test{ false };
	bool write{ true };
	std::uint32_t func{ GL_LESS };
	float range_near{ 0.0f };
	float range_far{ 1.0f };

	bool operator==(const DepthState&) const = default;
};

struct ColorMaskState {
	bool red{ true };
	bool green{ true };
	bool blue{ true };
	bool alpha{ true };

	bool operator==(const ColorMaskState&) const = default;
};

struct ScissorState {
	bool enabled{ false };
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const ScissorState&) const = default;
};

struct CullState {
	bool enabled{ false };
	std::uint32_t cull_face{ GL_BACK };
	std::uint32_t front_face{ GL_CCW };

	bool operator==(const CullState&) const = default;
};

struct PolygonModeFront {
	PolygonModeFront() = default;

	PolygonModeFront(std::uint32_t value) : value{ value } {}

	std::uint32_t value{ GL_FILL };

	operator std::uint32_t() const {
		return value;
	}
};

struct PolygonModeBack {
	PolygonModeBack() = default;

	PolygonModeBack(std::uint32_t value) : value{ value } {}

	std::uint32_t value{ GL_FILL };

	operator std::uint32_t() const {
		return value;
	}
};

struct LineWidth {
	LineWidth() = default;

	LineWidth(float value) : value{ value } {}

	float value{ 1.0f };

	operator float() const {
		return value;
	}
};

struct RasterState {
	CullState cull;
	PolygonModeFront polygon_mode_front{ GL_FILL };
	PolygonModeBack polygon_mode_back{ GL_FILL };
	LineWidth line_width{ 1.0f };
	bool line_smoothing{ false };

	bool operator==(const RasterState&) const = default;
};

using TextureUnits = std::vector<TextureUnitState>;

struct BlendState {
	BlendState() = default;

	BlendState(BlendMode mode, bool enabled) : mode{ mode }, enabled{ enabled } {}

	BlendMode mode{ BlendMode::ReplaceRGBA };
	bool enabled{ false };

	operator bool() const {
		return enabled;
	}

	bool operator==(const BlendState&) const = default;
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

using StateChange = std::variant<
	FramebufferId, RenderbufferId, VertexBufferId, UniformBufferId, ShaderId, VertexArrayId,
	Viewport, DepthState, BlendState, ColorMaskState, ActiveTextureSlot, TextureUnits, ClearColor,
	ClearDepth, ClearStencil, ScissorState, PolygonModeFront, PolygonModeBack, LineWidth, CullState,
	StencilState>;

struct State {
	// Core object bindings
	FramebufferId framebuffer;
	RenderbufferId renderbuffer;
	VertexBufferId vertex_buffer;
	UniformBufferId uniform_buffer;
	ShaderId shader;
	VertexArrayId vertex_array;

	Viewport viewport;

	DepthState depth;

	BlendState blend;

	ColorMaskState color_mask;

	ActiveTextureSlot active_texture_slot;
	TextureUnits texture_units;

	ClearColor clear_color;
	ClearDepth clear_depth;
	ClearStencil clear_stencil;

	ScissorState scissor;

	// Polygon rasterization
	RasterState raster;

	StencilState stencil;

	bool operator==(const State&) const = default;
};

} // namespace ptgn::impl::gl
