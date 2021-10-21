#pragma once

namespace ptgn {

namespace impl {

class SDLManager {
public:
	SDLManager();
    ~SDLManager();
};

// Calling this function ensures that all SDL systems have been initialized.
SDLManager& GetSDLManager();

} // namespace impl

} // namespace ptgn