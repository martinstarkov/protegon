#include "tools/debug/profiling.h"

#include <chrono>
#include <list>
#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/time/time.h"
#include "core/time/timer.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name) :
	name_{ function_name }, timer_{ true } {}

ProfileInstance::~ProfileInstance() {
	PTGN_ASSERT(!name_.empty());
	auto& time = GetProfiler().timings_.emplace(name_, 0ns).first->second;
	auto elapsed{ timer_.ElapsedDuration<nanoseconds>() };
	time += elapsed;
}

Profiler& GetProfiler() {
	static Profiler profiler;
	return profiler;
}

} // namespace ptgn::impl