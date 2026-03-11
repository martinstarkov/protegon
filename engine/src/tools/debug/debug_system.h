#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

#include "core/math/geometry/origin.h"
#include "core/math/geometry/shape.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "renderer/primitives/blend_mode.h"
#include "renderer/primitives/color.h"
#include "renderer/primitives/render_state.h"
#include "runtime/graphics/camera.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/font.h"
#include "runtime/graphics/text.h"
#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"

namespace ptgn {

class RenderContext;
class Application;

class DebugContext {
public:
	DebugContext()									 = delete;
	~DebugContext()									 = default;
	DebugContext(const DebugContext&)				 = delete;
	DebugContext& operator=(const DebugContext&)	 = delete;
	DebugContext(DebugContext&&) noexcept			 = delete;
	DebugContext& operator=(DebugContext&&) noexcept = delete;

	/// @param text_size {} results in unscaled size of text based on font.
	void DrawText(
		std::string_view content, Transform transform, Color text_color,
		std::optional<float> font_size									 = {},
		const std::variant<std::monostate, Font, std::string_view>& font = {},
		const TextProperties& properties = {}, Origin origin = Origin::Center,
		std::optional<V2_float> text_size = {}, bool hd_text = true,
		std::optional<Camera> camera = {}
	);

	/// @param origin only applicable to Rect and RoundedRect.
	void DrawShape(
		const Shape& shape, Transform transform, Color color,
		FillStyle fill_style = FillStyle::Hollow(1.0f), Origin draw_origin = Origin::Center,
		std::optional<Camera> camera = {}
	);

	void DrawLines(
		const std::vector<V2_float>& points, Color color, float line_width = kMinLineWidth,
		bool connect_last_to_first = false, std::optional<Transform> transform = {},
		std::optional<Camera> camera = {}
	);

	void DrawLine(
		V2_float start, V2_float end, Color color, float line_width = 1.0f,
		std::optional<Camera> camera = {}
	);

	void DrawPoint(V2_float point, Color color, std::optional<Camera> camera = {});

private:
	friend class Scene;

	std::optional<BlendMode> debug_blend_mode;

	explicit DebugContext(RenderContext& render_context);

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
