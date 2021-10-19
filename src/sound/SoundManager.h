#pragma once

#include <cstdint> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::shared_ptr

namespace ptgn {

namespace interfaces {

class SoundManager {
public:
    virtual void LoadSound(const char* sound_key, const char* sound_path) = 0;
    virtual void UnloadSound(const char* sound_key) = 0;
};

} // namespace interface

namespace impl {

// class SDLRenderer;

class SDLSoundManager : public interfaces::SoundManager {
public:
    SDLSoundManager() = default;
    ~SDLSoundManager();
    virtual void LoadSound(const char* sound_key, const char* sound_path) override;
    virtual void UnloadSound(const char* sound_key) override;
private:
    // friend class SDLRenderer;
    // std::shared_ptr<SDL_Sound> GetSound(const char* sound_key);
	std::unordered_map<std::size_t, void*> sound_map_;
};

SDLSoundManager& GetSDLSoundManager();

} // namespace impl

namespace services {

interfaces::SoundManager& GetSoundManager();

} // namespace services

} // namespace ptgn