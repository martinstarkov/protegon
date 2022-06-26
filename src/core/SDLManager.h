#pragma once

namespace ptgn {

namespace internal {

class SDLManager {
public:
	SDLManager();
    ~SDLManager();
};

// Calling this function ensures that all SDL systems have been initialized.
SDLManager& GetSDLManager();

} // namespace internal

} // namespace ptgn