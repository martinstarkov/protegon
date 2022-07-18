#include "Engine.h"

#include "interface/Draw.h"
#include "interface/Input.h"
#include "interface/Window.h"

namespace ptgn {

void Engine::Start(const char* window_title, const V2_int& window_size) {
    window::Create(window_title, window_size);
    InternalInit();
}

void Engine::InternalInit() {
    start = std::chrono::system_clock::now();
	end = std::chrono::system_clock::now();

    Init();
    InternalUpdate();
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