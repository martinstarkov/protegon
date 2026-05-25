#include "tools/debug/debug_system.h"

#include "tools/debug/profiling.h"
#include "tools/debug/stats.h"

namespace ptgn {

void DebugSystem::PreUpdate() {
	impl::GetProfiler().timings_.clear();
}

void DebugSystem::PostUpdate() {
	stats.Reset();
}

} // namespace ptgn