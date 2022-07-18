#pragma once

namespace ptgn {

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

} // namespace ptgn