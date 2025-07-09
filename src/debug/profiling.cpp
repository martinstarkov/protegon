#include "profiling.h"

#include <string>
#include <string_view>

#include "core/game.h"
#include "core/resource_manager.h"
#include "core/timer.h"
#include "math/hash.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name) :
	name_{ function_name }, timer_{ true } {}

ProfileInstance::~ProfileInstance() {
	PTGN_ASSERT(!name_.empty());
	auto& time = game.profiler.Load(name_);
	auto elapsed{ timer_.Elapsed<nanoseconds>() };
	time += elapsed;
}

} // namespace ptgn::impl