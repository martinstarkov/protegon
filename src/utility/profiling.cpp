#include "profiling.h"

#include <string>
#include <string_view>

#include "core/manager.h"
#include "protegon/game.h"
#include "protegon/hash.h"
#include "protegon/timer.h"

namespace ptgn {

namespace impl {

ProfileInstance::ProfileInstance(std::string_view function_name, std::string_view custom_name) :
	name_{ custom_name.empty() ? function_name : custom_name } {
	game.profiler.Load(name_).Start();
}

ProfileInstance::~ProfileInstance() {
	if (!name_.empty()) {
		game.profiler.Get(name_).Stop();
	}
}

} // namespace impl

} // namespace ptgn
