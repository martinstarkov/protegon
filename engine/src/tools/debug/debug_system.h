#pragma once

#include "tools/debug/allocation.h"
#include "tools/debug/stats.h"

namespace ptgn {

class Application;

namespace impl {

class ApplicationContext;

} // namespace impl

class DebugSystem {
public:
	impl::Allocations allocations;
	Stats stats;

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
	void PostUpdate();
};

} // namespace ptgn
