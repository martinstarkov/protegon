#pragma once

#include <string>

#include "core/asset/asset_handle.h"
#include "ecs/components/draw.h"
#include "ecs/components/generic.h"
#include "debug/allocation.h"
#include "debug/stats.h"
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
class Renderer;

namespace impl {

class DebugSystem {
public:
	DebugSystem(Renderer& renderer);
	~DebugSystem() noexcept						   = default;
	DebugSystem(const DebugSystem&)				   = delete;
	DebugSystem& operator=(const DebugSystem&)	   = delete;
	DebugSystem(DebugSystem&&) noexcept			   = delete;
	DebugSystem& operator=(DebugSystem&&) noexcept = delete;

	Allocations allocations;
	Stats stats;

	// @param text_size {} results in unscaled size of text based on font.
	// TODO: Fix.
	/*void DrawText(
		const std::string& content, const Transform& transform,
		const TextColor& color = color::White, Origin origin = Origin::Center,
		const FontSize& font_size = {}, const Handle<Font>& font_key = {},
		const TextProperties& properties = {}, const V2_float& text_size = {},
		const Camera& camera = {}
	);*/

	// @param origin only applicable to Rect and RoundedRect.
	/*
	void DrawShape(
		const Transform& transform, const Shape& shape, const Tint& color,
		const LineWidth& line_width = {}, Origin origin = Origin::Center, const Camera& camera = {}
	);

	void DrawLine(
		const V2_float& start, const V2_float& end, const Tint& color,
		const LineWidth& line_width = {}, const Camera& camera = {}
	);

	void DrawPoint(const V2_float& point, const Tint& color, const Camera& camera = {});
	*/

private:
	friend class Application;

	void PreUpdate();
	void PostUpdate();

	Renderer& renderer_;
};

} // namespace impl

} // namespace ptgn
