#include "debug/runtime/profiling.h"

#include <string>
#include <string_view>

#include "core/app/Application.h"
#include "core/asset/asset_manager.h"
#include "core/util/timer.h"
#include "debug/runtime/debug_system.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name) :
	name_{ function_name }, timer_{ true } {}

ProfileInstance::~ProfileInstance() {
	PTGN_ASSERT(!name_.empty());
	auto& time = Application::Get().debug_.profiler.Load(name_);
	auto elapsed{ timer_.Elapsed<nanoseconds>() };
	time += elapsed;
}

} // namespace ptgn::impl