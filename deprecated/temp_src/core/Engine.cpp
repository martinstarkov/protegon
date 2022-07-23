#include "Engine.h"

#include "math/Hash.h"
#include "renderer/Renderer.h"
#include "core/Window.h"
#include "event/Input.h"

namespace ptgn {

void Engine::Start(const char* window_title, const V2_int& window_size) {
    window::Create(window_title, window_size);
    Renderer::Create();
    InternalInit();
}

void Engine::Stop() {
    window::Destroy();
    Renderer::Destroy();
}

void Engine::InternalUpdate() {
    while (window::IsValid()) {
        // Calculate time elapsed during previous frame.
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        start = end;
        double dt = elapsed.count();
        
        // Clear screen.
        Renderer::Clear();
        Renderer::SetDrawColor(window::GetColor());
        
        // Fetch updated user inputs.
        input::Update();

        // Call user update.
        Update(dt);

        // Push drawn objects to screen.
        Renderer::Present();
    }
}

} // namespace ptgn