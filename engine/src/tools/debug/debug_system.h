#pragma once

#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "runtime/ecs/entity.h"
#include "serialization/serialize.h"
#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"

namespace ptgn {

class Application;
class Scene;
class SceneCamera;
class RenderTarget;
struct Camera;

namespace impl {

class ApplicationContext;

} // namespace impl

struct InteractiveDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	float draw_line_width{ 2.0f };

	PTGN_SERIALIZE(InteractiveDebugSettings, draw_enabled, draw_color, draw_line_width)
};

struct CollisionDebugSettings {
	/// @brief If true, draws continuous collision detection sweeps for debugging purposes.
	bool draw_ccd{ false };

	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	FillStyle draw_fill_style{ 1.0f };

	[[nodiscard]] bool DrawCCD() const {
		return draw_enabled && draw_ccd;
	}

	PTGN_SERIALIZE(CollisionDebugSettings, draw_ccd, draw_enabled, draw_color, draw_fill_style)
};

struct TextDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	Color clip_draw_color{ color::Blue };
	float draw_line_width{ 2.0f };

	PTGN_SERIALIZE(TextDebugSettings, draw_enabled, draw_color, clip_draw_color, draw_line_width)
};

struct LightVisibilityDebugSettings {
	bool draw_enabled{ true };
	bool draw_interiors{ true };

	Color polygon_color{ color::Yellow };
	Color masks_inside_color{ color::Red };
	Color does_not_mask_inside_color{ color::Green };

	FillStyle draw_fill_style{ 2.0f };

	PTGN_SERIALIZE(
		LightVisibilityDebugSettings, draw_enabled, draw_interiors, polygon_color,
		masks_inside_color, does_not_mask_inside_color, draw_fill_style
	)
};

class DebugSystem {
public:
	impl::Allocations allocations;
	Stats stats;
	InteractiveDebugSettings interaction;
	CollisionDebugSettings collision;
	TextDebugSettings text;
	LightVisibilityDebugSettings light;

private:
	friend class Application;
	friend class impl::ApplicationContext;

	DebugSystem()								   = default;
	~DebugSystem() noexcept						   = default;
	DebugSystem(const DebugSystem&)				   = delete;
	DebugSystem& operator=(const DebugSystem&)	   = delete;
	DebugSystem(DebugSystem&&) noexcept			   = delete;
	DebugSystem& operator=(DebugSystem&&) noexcept = delete;

	void PreUpdate();
	void PostRender();
};

namespace impl {

void DrawDebug(
	Scene& scene, const SceneCamera& camera, const Camera& cam, const RenderTarget& render_target,
	const impl::EntityFilterFunc& filter, const DebugSystem& debug
);

} // namespace impl

} // namespace ptgn
