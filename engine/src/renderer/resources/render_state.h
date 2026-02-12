#pragma once

#include <cstdint>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/math/vector2.h"

namespace ptgn {

struct Viewport {
	// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const Viewport&) const = default;
};

//
// Comparison function (glStencilFunc, glDepthFunc)
//
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

//
// Stencil operations (glStencilOp / GL_STENCIL_FAIL, etc.)
//
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

struct DepthState {
	bool test{ false };
	bool write{ true };

	CompareFunc func{ CompareFunc::Less };

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

//
// Cull face selection (glCullFace)
//
enum class CullFace : std::uint32_t {
	Front		 = 0x0404, // GL_FRONT
	Back		 = 0x0405, // GL_BACK
	FrontAndBack = 0x0408  // GL_FRONT_AND_BACK
};

//
// Front face winding order (glFrontFace)
//
enum class FrontFace : std::uint32_t {
	CW	= 0x0900, // GL_CW, Clockwise
	CCW = 0x0901  // GL_CCW, Counter-clockwise
};

struct CullState {
	bool enabled{ false };

	CullFace cull_face{ CullFace::Back };
	FrontFace front_face{ FrontFace::CCW };

	bool operator==(const CullState&) const = default;
};

//
// Polygon rasterization mode (glPolygonMode)
//
enum class PolygonMode : std::uint32_t {
	Point = 0x1B00, // GL_POINT
	Line  = 0x1B01, // GL_LINE
	Fill  = 0x1B02	// GL_FILL
};

struct PolygonState {
	PolygonMode front{ PolygonMode::Fill };
	PolygonMode back{ PolygonMode::Fill };

	bool operator==(const PolygonState&) const = default;
};

struct LineWidth {
	LineWidth() = default;

	explicit LineWidth(float value) : value{ value } {}

	float value{ 1.0f };

	bool operator==(const LineWidth&) const = default;
};

struct RasterState {
	CullState cull;
	PolygonState polygon;
	LineWidth line_width{ 1.0f };
	bool line_smoothing{ false };

	bool operator==(const RasterState&) const = default;
};

struct BlendState {
	BlendState() = default;

	BlendState(BlendMode mode, bool enabled) : mode{ mode }, enabled{ enabled } {}

	BlendMode mode{ BlendMode::ReplaceRGBA };
	bool enabled{ false };

	bool operator==(const BlendState&) const = default;
};

struct ClearColor {
	ClearColor() = default;

	explicit ClearColor(Color value) : value{ value } {}

	Color value;
};

struct ClearDepth {
	ClearDepth() = default;

	explicit ClearDepth(double value) : value{ value } {}

	double value{ 0.0 };
};

struct ClearStencil {
	ClearStencil() = default;

	explicit ClearStencil(int value) : value{ value } {}

	int value{ 0 };
};

} // namespace ptgn
