#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::unique_ptr

struct Mix_Music;
struct Mix_Chunk;

namespace ptgn {

namespace interfaces {

class SoundManager {
public:
    virtual void LoadSound(const std::size_t sound_key, const char* sound_path) = 0;
    virtual void LoadMusic(const std::size_t music_key, const char* music_path) = 0;
    virtual void UnloadSound(const std::size_t sound_key) = 0;
    virtual void UnloadMusic(const std::size_t music_key) = 0;
    virtual bool HasSound(const std::size_t sound_key) const = 0;
    virtual bool HasMusic(const std::size_t music_key) const = 0;
};

} // namespace interface

namespace impl {

struct SDLSoundDeleter {
    void operator()(Mix_Chunk* sound);
};

struct SDLMusicDeleter {
    void operator()(Mix_Music* music);
};

class SDLSoundManager : public interfaces::SoundManager {
public:
    SDLSoundManager();
    ~SDLSoundManager() = default;
    virtual void LoadSound(const std::size_t key, const char* path) override;
    virtual void LoadMusic(const std::size_t key, const char* path) override;
    virtual void UnloadSound(const std::size_t key) override;
    virtual void UnloadMusic(const std::size_t key) override;
    virtual bool HasSound(const std::size_t key) const override;
    virtual bool HasMusic(const std::size_t key) const override;
    void SetSound(const std::size_t key, Mix_Chunk* sound);
    void SetMusic(const std::size_t key, Mix_Music* music);
    Mix_Chunk* GetSound(const std::size_t key);
    Mix_Music* GetMusic(const std::size_t key);
	std::unordered_map<std::size_t, std::unique_ptr<Mix_Chunk, SDLSoundDeleter>> sound_map_;
	std::unordered_map<std::size_t, std::unique_ptr<Mix_Music, SDLMusicDeleter>> music_map_;
};

SDLSoundManager& GetSDLSoundManager();

} // namespace impl

namespace services {

interfaces::SoundManager& GetSoundManager();

} // namespace services

} // namespace ptgn