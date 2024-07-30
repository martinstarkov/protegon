#include "profiling.h"

#include "protegon/game.h"
#include "protegon/hash.h"

namespace ptgn {

namespace impl {

ProfileInstance::ProfileInstance(const std::string& function_name, const std::string& custom_name) :
	name_{ custom_name == "" ? function_name : custom_name } {
	game.profiler.Load(name_).Start();
}

ProfileInstance::~ProfileInstance() {
	if (name_ != std::string{}) {
		game.profiler.Get(name_).Stop();
	}
}

} // namespace impl

} // namespace ptgn
