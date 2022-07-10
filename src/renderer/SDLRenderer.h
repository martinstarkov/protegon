#pragma once

struct SDL_Renderer;

namespace ptgn {

struct SDLRenderer {
    static SDLRenderer& Get() {
        static SDLRenderer renderer;
        return renderer;
    }
    SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn