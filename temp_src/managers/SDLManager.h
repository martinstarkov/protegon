#pragma once

#include "managers/ResourceManager.h"

namespace ptgn {

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
private:
};

} // namespace managers

} // namespace ptgn