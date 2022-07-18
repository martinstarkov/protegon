#pragma once

struct SDL_Renderer;

namespace ptgn {

struct Renderer {
    static Renderer& Get() {
        static Renderer renderer;
        return renderer;
    }
    SDL_Renderer* renderer_{ nullptr };
};

} // namespace ptgn