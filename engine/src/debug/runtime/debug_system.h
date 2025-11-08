#pragma once

#include <string>

#include "core/ecs/components/draw.h"
#include "core/ecs/components/generic.h"
#include "debug/runtime/allocation.h"
#include "debug/runtime/profiling.h"
#include "debug/runtime/stats.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/text/font.h"
#include "renderer/text/text.h"
#include "world/scene/camera.h"

namespace ptgn {

struct Transform;
class Shape;
class Application;

namespace impl {

class DebugSystem {
public:
	Allocations allocations;
	Profiler profiler;
	Stats stats;

	// @param text_size {} results in unscaled size of text based on font.
	void DrawText(
		const std::string& content, const Transform& transform,
		const TextColor& color = color::White, Origin origin = Origin::Center,
		const FontSize& font_size = {}, const ResourceHandle& font_key = {},
		const TextProperties& properties = {}, const V2_float& text_size = {},
		const Camera& camera = {}
	);

	// @param origin only applicable to Rect and RoundedRect.
	void DrawShape(
		const Transform& transform, const Shape& shape, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = Origin::Center, const Camera& camera = {}
	);

	void DrawLine(
		const V2_float& start, const V2_float& end, const Tint& color,
		const LineWidth& line_width = {}, const Camera& camera = {}
	);

	void DrawPoint(const V2_float& point, const Tint& color, const Camera& camera = {});

private:
	friend class Application;

	void PreUpdate();
	void PostUpdate();
};

} // namespace impl

} // namespace ptgn
