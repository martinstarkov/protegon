#pragma once

#include <cstdint>

#include "core/math/tolerance.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/viewport.h"
#include "serialization/serialize.h"

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

PTGN_REFLECT_ENUM(CompareFunc);

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

PTGN_REFLECT_ENUM(StencilOp);

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

	PTGN_REFLECT(StencilState, enabled, func, ref, mask, fail_op, zfail_op, zpass_op, write_mask)
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

	PTGN_REFLECT(DepthMaskState, write, func, range_near, range_far)
};

struct ClearDepth {
	double value{ 1.0 };

	bool operator==(const ClearDepth& other) const {
		return NearlyEqual(value, other.value);
	}

	PTGN_REFLECT(ClearDepth, value)
};

struct ColorMaskState {
	bool red{ true };
	bool green{ true };
	bool blue{ true };
	bool alpha{ true };

	bool operator==(const ColorMaskState&) const = default;

	PTGN_REFLECT(ColorMaskState, red, green, blue, alpha)
};

struct ScissorState {
	ScissorState() = default;

	explicit ScissorState(Viewport viewport) : viewport{ viewport }, enabled{ true } {}

	explicit ScissorState(bool enabled) : enabled{ enabled } {}

	/// @brief Viewport of the scissor rectangle.
	Viewport viewport;

	bool enabled{ false };

	bool operator==(const ScissorState&) const = default;

	PTGN_REFLECT(ScissorState, enabled, viewport)
};

/// @brief Cull face selection (glCullFace)
enum class CullFace : std::uint32_t {
	Front		 = 0x0404, // GL_FRONT
	Back		 = 0x0405, // GL_BACK
	FrontAndBack = 0x0408  // GL_FRONT_AND_BACK
};

PTGN_REFLECT_ENUM(CullFace);

/// @brief Front face winding order (glFrontFace)
enum class FrontFace : std::uint32_t {
	CW	= 0x0900, // GL_CW, Clockwise
	CCW = 0x0901  // GL_CCW, Counter-clockwise
};

PTGN_REFLECT_ENUM(FrontFace);

struct CullState {
	bool enabled{ false };

	CullFace cull_face{ CullFace::Back };
	FrontFace front_face{ FrontFace::CCW };

	bool operator==(const CullState&) const = default;

	PTGN_REFLECT(CullState, enabled, cull_face, front_face)
};

struct RasterState {
	CullState cull;
	float line_width{ 1.0f };

	bool operator==(const RasterState& other) const {
		return cull == other.cull && NearlyEqual(line_width, other.line_width);
	}

	PTGN_REFLECT(RasterState, cull, line_width)
};

} // namespace ptgn
