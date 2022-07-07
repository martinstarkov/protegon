#include "Engine.h"

#include "math/Hash.h"
#include "managers/WindowManager.h"
#include "managers/RendererManager.h"
#include "event/Input.h"

namespace ptgn {

void Engine::Start(const char* key, const char* window_title, const V2_int& window_size) {
    key_ = math::Hash(key);
    auto& window_manager{ managers::GetManager<managers::WindowManager>() };
    window_manager.Load(key_, new Window{ window_title, window_size });
    auto window = window_manager.Get(key_);
    managers::GetManager<managers::RendererManager>().Load(key_, new Renderer{ *window, 0, 0 });
    InternalInit();
}

void Engine::InternalInit() {
    start = std::chrono::system_clock::now();
	end = std::chrono::system_clock::now();

    Init();
    InternalUpdate();
}

void Engine::InternalUpdate() {
    const auto& window_manager{ managers::GetManager<managers::WindowManager>() };
    const auto& renderer_manager{ managers::GetManager<managers::RendererManager>() };
    while (window_manager.Has(key_) && 
           window_manager.Get(key_) != nullptr) {
        auto window = window_manager.Get(key_);
        // Calculate time elapsed during previous frame.
        end = std::chrono::system_clock::now();
        std::chrono::duration<double> elapsed = end - start;
        start = end;
        double dt = elapsed.count();
        
        bool draw = renderer_manager.Has(key_);
        auto renderer = renderer_manager.Get(key_);
        draw &= renderer != nullptr;
        if (draw) {
            // Clear screen.
            renderer->Clear();
            renderer->SetDrawColor(window->GetColor());
        }
        
        // Fetch updated user inputs.
        input::Update();

        // Call user update.
        Update(dt);

        if (draw) {
            // Push drawn objects to screen.
            renderer->Present();
        }
    }
}

} // namespace ptgn