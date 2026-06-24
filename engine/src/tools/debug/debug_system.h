#pragma once

#include "core/graphics/color.h"
#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"

namespace ptgn {

class Application;

namespace impl {

class ApplicationContext;

} // namespace impl

struct TextDebugSettings {
	bool draw_enabled{ false };
	Color draw_color{ color::Magenta };
	Color clip_draw_color{ color::Blue };
	float draw_line_width{ 2.0f };
};

class DebugSystem {
public:
	impl::Allocations allocations;
	Stats stats;
	TextDebugSettings text;

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

} // namespace ptgn
