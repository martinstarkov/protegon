#pragma once

#include "managers/ResourceManager.h"

namespace ptgn {

namespace managers {

template <typename T>
class SDLManager : public ResourceManager<T> {
public:
	SDLManager() {
		GetManager<SDLSystemManager>();
	}
private:
	class SDLSystemManager {
	public:
		SDLSystemManager();
		~SDLSystemManager();
	};
};

} // namespace managers

} // namespace ptgn