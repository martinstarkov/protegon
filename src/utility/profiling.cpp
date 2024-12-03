#include "profiling.h"

#include <string>
#include <string_view>

#include "core/manager.h"
#include "core/game.h"
#include "math/hash.h"
#include "utility/timer.h"

namespace ptgn::impl {

ProfileInstance::ProfileInstance(std::string_view function_name, std::string_view custom_name) :
	name_{ custom_name.empty() ? function_name : custom_name } {
	game.profiler.Load(name_).Start();
}

ProfileInstance::~ProfileInstance() {
	if (!name_.empty()) {
		game.profiler.Get(name_).Stop();
	}
}

} // namespace ptgn::impl