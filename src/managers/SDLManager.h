#pragma once

namespace ptgn {

namespace internal {

namespace managers {

class SDLManager {
public:
	SDLManager();
    ~SDLManager();
};

} // namespace managers

// Calling this function ensures that all SDL systems have been initialized.
managers::SDLManager& GetSDLManager();

} // namespace internal

} // namespace ptgn