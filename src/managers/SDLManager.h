#pragma once

namespace ptgn {

namespace managers {

class SDLManager {
public:
	static SDLManager& Get() {
		static SDLManager instance;
		return instance;
	}
private:
	SDLManager();
	~SDLManager();
};

} // namespace managers

} // namespace ptgn