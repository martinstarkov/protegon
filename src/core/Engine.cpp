#include "protegon/engine.h"

/*
namespace ptgn {

Engine::Engine() {
    SDLManager::Get();
    window::Init("", {}, {}, window::Flags::HIDDEN);
    draw::Init();
}

void Engine::Start(const char* window_title, 
                   const V2_int& window_size, 
                   bool window_centered, 
                   V2_int& window_position,
                   window::Flags fullscreen_flag,
                   bool resizeable,
                   bool maximize) {
    if (window_centered) {
        window_position = window::CENTERED;
    }
    window::SetTitle(window_title);
    window::SetSize(window_size);
    window::SetOriginPosition(window_position);
    window::SetResizeable(resizeable);
    window::SetFullscreen(fullscreen_flag);
    if (maximize)
        window::Maximize();
    window::Show();
    InternalInit();
    Stop();
}

void Engine::Stop() {
    window::Release();
    draw::Release();
    manager::Get<SceneManager>().Clear();
    manager::Get<TextManager>().Clear();
    manager::Get<FontManager>().Clear();
    manager::Get<TextureManager>().Clear();
    manager::Get<MusicManager>().Clear();
    manager::Get<SoundManager>().Clear();
}

void Engine::InternalUpdate() {
    input::Update();
    while (window::Exists()) {
        // Calculate time elapsed during previous frame.
        end = std::chrono::system_clock::now();
        std::chrono::duration<float> elapsed = end - start;
        start = end;
        float dt = elapsed.count();

        draw::SetColor(window::GetColor());
        // Clear screen.
        draw::Clear();

        // Call user update.
        Update(dt);

        // Push drawn objects to screen.
        draw::Present();

        // Fetch updated user inputs.
        input::Update();
    }
}

} // namespace ptgn
*/