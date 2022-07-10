#include "Engine.h"

#include "renderer/Renderer.h"
#include "core/Window.h"
#include "input/Input.h"

namespace ptgn {

void Engine::Start(const char* window_title, const V2_int& window_size) {
    window::Init(window_title, window_size);
    draw::Init();
    InternalInit();
}

void Engine::Stop() {
    window::Release();
    draw::Release();
}

void Engine::InternalUpdate() {
    while (window::Exists()) {
        // Calculate time elapsed during previous frame.
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        start = end;
        double dt = elapsed.count();
        
        // Clear screen.
        draw::Clear();
        draw::SetColor(window::GetColor());
        
        // Fetch updated user inputs.
        input::Update();

        // Call user update.
        Update(dt);

        // Push drawn objects to screen.
        draw::Present();
    }
}

} // namespace ptgn