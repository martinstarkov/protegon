#pragma once

#include "managers/ResourceManager.h"

namespace ptgn {

namespace internal {

namespace managers {

class SDLSystemManager {
public:
	SDLSystemManager();
	~SDLSystemManager();
};

template <typename T>
class SDLManager : public ResourceManager<T> {
public:
	SDLManager() {
		GetManager<SDLSystemManager>();
	}
};

} // namespace managers

} // namespace internal

} // namespace ptgn