#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/math/matrix4.h"
#include "core/math/tolerance.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/viewport.h"
#include "renderer/resources/id.h"
#include "renderer/resources/shader.h"

namespace ptgn {

/// @brief Comparison function (glStencilFunc, glDepthFunc)
enum class CompareFunc : std::uint32_t {
	Never	 = 0x0200, // GL_NEVER
	Less	 = 0x0201, // GL_LESS
	Equal	 = 0x0202, // GL_EQUAL
	LEqual	 = 0x0203, // GL_LEQUAL
	Greater	 = 0x0204, // GL_GREATER
	NotEqual = 0x0205, // GL_NOTEQUAL
	GEqual	 = 0x0206, // GL_GEQUAL
	Always	 = 0x0207  // GL_ALWAYS
};

/// @brief Stencil operations (glStencilOp / GL_STENCIL_FAIL, etc.)
enum class StencilOp : std::uint32_t {
	Keep	 = 0x1E00, // GL_KEEP
	Zero	 = 0x0000, // GL_ZERO
	Replace	 = 0x1E01, // GL_REPLACE
	Incr	 = 0x1E02, // GL_INCR
	IncrWrap = 0x8507, // GL_INCR_WRAP
	Decr	 = 0x1E03, // GL_DECR
	DecrWrap = 0x8508, // GL_DECR_WRAP
	Invert	 = 0x150A  // GL_INVERT
};

/// @brief Cull face selection (glCullFace)
enum class CullFace : std::uint32_t {
	Front		 = 0x0404, // GL_FRONT
	Back		 = 0x0405, // GL_BACK
	FrontAndBack = 0x0408  // GL_FRONT_AND_BACK
};

/// @brief Front face winding order (glFrontFace)
enum class FrontFace : std::uint32_t {
	CW	= 0x0900, // GL_CW, Clockwise
	CCW = 0x0901  // GL_CCW, Counter-clockwise
};

struct StencilState {
	bool enabled{ false };

	CompareFunc func{ CompareFunc::Always };

	int ref{ 0 };

	std::uint32_t mask{ 0xFFFFFFFF };

	StencilOp fail_op{ StencilOp::Keep };
	StencilOp zfail_op{ StencilOp::Keep };
	StencilOp zpass_op{ StencilOp::Keep };

	std::uint32_t write_mask{ 0xFFFFFFFF };

	bool operator==(const StencilState&) const = default;
};

struct DepthMaskState {
	bool write{ true };

	CompareFunc func{ CompareFunc::Less };

	float range_near{ 0.0f };
	float range_far{ 1.0f };

	bool operator==(const DepthMaskState& other) const {
		return write == other.write && func == other.func &&
			   NearlyEqual(range_near, other.range_near) && NearlyEqual(range_far, other.range_far);
	}
};

struct ClearDepth {
	double value{ 1.0 };

	bool operator==(const ClearDepth& other) const {
		return NearlyEqual(value, other.value);
	}
};

struct ColorMaskState {
	bool red{ true };
	bool green{ true };
	bool blue{ true };
	bool alpha{ true };

	bool operator==(const ColorMaskState&) const = default;
};

struct ScissorState {
	ScissorState() = default;

	explicit ScissorState(Viewport viewport) : viewport{ viewport }, enabled{ true } {}

	explicit ScissorState(bool enabled) : enabled{ enabled } {}

	/// @brief Viewport of the scissor rectangle.
	Viewport viewport;

	bool enabled{ false };

	bool operator==(const ScissorState&) const = default;
};

struct CullState {
	bool enabled{ false };

	CullFace cull_face{ CullFace::Back };
	FrontFace front_face{ FrontFace::CCW };

	bool operator==(const CullState&) const = default;
};

struct RasterState {
	CullState cull;
	float line_width{ 1.0f };

	bool operator==(const RasterState& other) const {
		return cull == other.cull && NearlyEqual(line_width, other.line_width);
	}
};

struct MaterialState {
	impl::ShaderId shader;
	std::vector<UniformWrite> uniforms;

	bool operator==(const MaterialState&) const = default;
};

struct RenderState {
	std::optional<Viewport> viewport;
	std::optional<Matrix4> view_projection;

	std::optional<BlendMode> blend_mode;

	std::optional<bool> depth_testing;
	std::optional<DepthMaskState> depth_mask;
	std::optional<StencilState> stencil;
	std::optional<RasterState> raster;
	std::optional<ScissorState> scissor;
	std::optional<ColorMaskState> color_mask;

	bool operator==(const RenderState&) const = default;
};

inline RenderState ApplyDelta(RenderState base, const RenderState& delta) {
	if (delta.view_projection.has_value()) {
		base.view_projection = *delta.view_projection;
	}
	if (delta.viewport.has_value()) {
		base.viewport = *delta.viewport;
	}
	if (delta.blend_mode.has_value()) {
		base.blend_mode = *delta.blend_mode;
	}
	if (delta.depth_testing.has_value()) {
		base.depth_testing = *delta.depth_testing;
	}
	if (delta.depth_mask.has_value()) {
		base.depth_mask = *delta.depth_mask;
	}
	if (delta.stencil.has_value()) {
		base.stencil = *delta.stencil;
	}
	if (delta.raster.has_value()) {
		base.raster = *delta.raster;
	}
	if (delta.scissor.has_value()) {
		base.scissor = *delta.scissor;
	}
	if (delta.color_mask.has_value()) {
		base.color_mask = *delta.color_mask;
	}
	return base;
}

} // namespace ptgn
