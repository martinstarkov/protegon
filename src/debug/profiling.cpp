#include "profiling.h"

#include <string>
#include <string_view>

#include "core/game.h"
#include "core/timer.h"
#include "debug/debug_system.h"
#include "math/hash.h"
#include "resources/resource_manager.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name) :
	name_{ function_name }, timer_{ true } {}

ProfileInstance::~ProfileInstance() {
	PTGN_ASSERT(!name_.empty());
	auto& time = game.debug.profiler.Load(name_);
	auto elapsed{ timer_.Elapsed<nanoseconds>() };
	time += elapsed;
}

} // namespace ptgn::impl