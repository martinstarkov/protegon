#pragma once

#include <cstdint>
#include <ostream>
#include <utility>

#include "core/graphics/blend_mode.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/vector2.h"

namespace ptgn {

/// Comparison function (glStencilFunc, glDepthFunc)
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

inline std::ostream& operator<<(std::ostream& os, CompareFunc func) {
	switch (func) {
		using enum CompareFunc;
		case Never:	   return os << "Never";
		case Less:	   return os << "Less";
		case Equal:	   return os << "Equal";
		case LEqual:   return os << "LEqual";
		case Greater:  return os << "Greater";
		case NotEqual: return os << "NotEqual";
		case GEqual:   return os << "GEqual";
		case Always:   return os << "Always";
		default:	   PTGN_ERROR("Unknown CompareFunc: ", std::to_underlying(func));
	}
}

/// Stencil operations (glStencilOp / GL_STENCIL_FAIL, etc.)
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

inline std::ostream& operator<<(std::ostream& os, StencilOp op) {
	switch (op) {
		using enum StencilOp;
		case Keep:	   return os << "Keep";
		case Zero:	   return os << "Zero";
		case Replace:  return os << "Replace";
		case Incr:	   return os << "Incr";
		case IncrWrap: return os << "IncrWrap";
		case Decr:	   return os << "Decr";
		case DecrWrap: return os << "DecrWrap";
		case Invert:   return os << "Invert";
		default:	   PTGN_ERROR("Unknown StencilOp: ", std::to_underlying(op));
	}
}

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

	friend std::ostream& operator<<(std::ostream& os, const StencilState& stencil) {
		if (stencil.enabled) {
			os << "(enabled=" << stencil.enabled << ", func=" << stencil.func
			   << ", ref=" << stencil.ref << ", mask=0x" << std::hex << stencil.mask << std::dec
			   << ", fail_op=" << stencil.fail_op << ", zfail_op=" << stencil.zfail_op
			   << ", zpass_op=" << stencil.zpass_op << ", write_mask=0x" << std::hex
			   << stencil.write_mask << std::dec << ")";
		} else {
			os << "(enabled=" << stencil.enabled << ")";
		}
		return os;
	}
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

	friend std::ostream& operator<<(std::ostream& os, const ColorMaskState& mask) {
		os << "(r=" << mask.red << ",g=" << mask.green << ",b=" << mask.blue << ",a=" << mask.alpha
		   << ")";
		return os;
	}
};

struct ScissorState {
	bool enabled{ false };
	/// Top left position.
	V2_int position;
	V2_int size;

	bool operator==(const ScissorState&) const = default;

	friend std::ostream& operator<<(std::ostream& os, const ScissorState& scissor) {
		if (scissor.enabled) {
			os << "(enabled=" << scissor.enabled << ", pos=" << scissor.position
			   << ", size=" << scissor.size << ")";
		} else {
			os << "(enabled=" << scissor.enabled << ")";
		}
		return os;
	}
};

/// Cull face selection (glCullFace)
enum class CullFace : std::uint32_t {
	Front		 = 0x0404, // GL_FRONT
	Back		 = 0x0405, // GL_BACK
	FrontAndBack = 0x0408  // GL_FRONT_AND_BACK
};

inline std::ostream& operator<<(std::ostream& os, CullFace face) {
	switch (face) {
		using enum CullFace;
		case Front:		   return os << "Front";
		case Back:		   return os << "Back";
		case FrontAndBack: return os << "FrontAndBack";
		default:		   PTGN_ERROR("Unknown CullFace: ", std::to_underlying(face));
	}
}

/// Front face winding order (glFrontFace)
enum class FrontFace : std::uint32_t {
	CW	= 0x0900, // GL_CW, Clockwise
	CCW = 0x0901  // GL_CCW, Counter-clockwise
};

inline std::ostream& operator<<(std::ostream& os, FrontFace face) {
	switch (face) {
		using enum FrontFace;
		case CW:  return os << "Clockwise";
		case CCW: return os << "Counter-clockwise";
		default:  PTGN_ERROR("Unknown FrontFace: ", std::to_underlying(face));
	}
}

struct CullState {
	bool enabled{ false };

	CullFace cull_face{ CullFace::Back };
	FrontFace front_face{ FrontFace::CCW };

	bool operator==(const CullState&) const = default;

	friend std::ostream& operator<<(std::ostream& os, const CullState& cull) {
		if (cull.enabled) {
			os << "(enabled=" << cull.enabled << ", cull_face=" << cull.cull_face
			   << ", front_face=" << cull.front_face << ")";
		} else {
			os << "(enabled=" << cull.enabled << ")";
		}
		return os;
	}
};

/// Polygon rasterization mode (glPolygonMode)
enum class PolygonMode : std::uint32_t {
	Point = 0x1B00, // GL_POINT
	Line  = 0x1B01, // GL_LINE
	Fill  = 0x1B02	// GL_FILL
};

inline std::ostream& operator<<(std::ostream& os, PolygonMode mode) {
	switch (mode) {
		using enum PolygonMode;
		case Point: return os << "Point";
		case Line:	return os << "Line";
		case Fill:	return os << "Fill";
		default:	PTGN_ERROR("Unknown PolygonMode: ", std::to_underlying(mode));
	}
}

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

	bool operator==(const ClearColor&) const = default;
};

struct ClearDepth {
	ClearDepth() = default;

	explicit ClearDepth(double value) : value{ value } {}

	double value{ 0.0 };

	bool operator==(const ClearDepth&) const = default;
};

struct ClearStencil {
	ClearStencil() = default;

	explicit ClearStencil(int value) : value{ value } {}

	int value{ 0 };

	bool operator==(const ClearStencil&) const = default;
};

} // namespace ptgn
