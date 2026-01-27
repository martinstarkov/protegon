#include "tools/debug/profiling.h"

#include <string>
#include <string_view>

#include "core/time/time.h"
#include "core/time/timer.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name) :
	name_{ function_name }, timer_{ true } {}

ProfileInstance::~ProfileInstance() {
	PTGN_ASSERT(!name_.empty());
	auto& time = GetProfiler().timings_.emplace(name_, nanoseconds{ 0 }).first->second;
	auto elapsed{ timer_.Elapsed<nanoseconds>() };
	time += elapsed;
}

} // namespace ptgn::impl