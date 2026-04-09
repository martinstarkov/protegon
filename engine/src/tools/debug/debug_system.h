#pragma once

#include <optional>
#include <string_view>
#include <variant>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "runtime/asset/asset.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"

namespace ptgn {

class RenderContext;
class Application;
class SceneContext;

class DebugContext {
public:
	/// @param text_size {} results in unscaled size of text based on font.
	void DrawText(
		std::string_view text_content, Transform transform, Color text_color,
		FontSize font_size = {}, FontOrKey font = {}, const TextProperties& properties = {},
		Origin draw_origin = Origin::Center, std::optional<V2_float> text_size = {},
		bool hd_text = true, const std::optional<Camera>& camera = {}
	);

	/// @param origin only applicable to Rect and RoundedRect.
	void DrawShape(
		const Shape& shape, Transform transform, Color color, FillStyle fill_style = 1.0f,
		Origin draw_origin = Origin::Center, const std::optional<Camera>& camera = {}
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = {},
		const std::optional<Camera>& camera = {}
	);

	void DrawLine(
		V2_float start, V2_float end, Color color, float line_width = 1.0f,
		const std::optional<Camera>& camera = {}
	);

	void DrawPoint(V2_float point, Color color, const std::optional<Camera>& camera = {});

private:
	friend class Scene;
	friend class SceneContext;

	DebugContext() = delete;
	explicit DebugContext(RenderContext& render_context);
	~DebugContext()									 = default;
	DebugContext(const DebugContext&)				 = delete;
	DebugContext& operator=(const DebugContext&)	 = delete;
	DebugContext(DebugContext&&) noexcept			 = default;
	DebugContext& operator=(DebugContext&&) noexcept = delete;

	Depth debug_depth;
	std::optional<BlendMode> debug_blend_mode;

	RenderContext& render_context_;
};

class DebugSystem {
public:
	DebugSystem();
	~DebugSystem() noexcept						   = default;
	DebugSystem(const DebugSystem&)				   = delete;
	DebugSystem& operator=(const DebugSystem&)	   = delete;
	DebugSystem(DebugSystem&&) noexcept			   = delete;
	DebugSystem& operator=(DebugSystem&&) noexcept = delete;

	impl::Allocations allocations;
	impl::Stats stats;

private:
	friend class ptgn::Application;

	void PreUpdate();
	void PostUpdate();
};

} // namespace ptgn
