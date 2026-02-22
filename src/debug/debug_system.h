#pragma once

#include <string>

#include "components/draw.h"
#include "components/generic.h"
#include "debug/allocation.h"
#include "debug/profiling.h"
#include "debug/stats.h"
#include "math/vector2.h"
#include "renderer/api/color.h"
#include "renderer/api/origin.h"
#include "renderer/font.h"
#include "renderer/text.h"
#include "scene/camera.h"

namespace ptgn {

struct Transform;
class Shape;

namespace impl {

class Game;

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
	friend class Game;

	void Shutdown();
	void PreUpdate();
	void PostUpdate();
};

} // namespace impl

} // namespace ptgn
