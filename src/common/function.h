#pragma once

#include <functional>
#include <utility>

namespace ptgn {

template <typename Callable, typename... Args>
static void Invoke(const Callable& callable, Args&&... args) {
	if (callable != nullptr) {
		std::invoke(callable, std::forward<Args>(args)...);
	}
}

} // namespace ptgn