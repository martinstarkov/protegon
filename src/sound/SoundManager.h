#pragma once

#include <cstddef> // std::size_t
#include <unordered_map> // std::unordered_map
#include <memory> // std::unique_ptr

#include "utils/Timer.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn {

namespace interfaces {

class SoundManager {
public:
    virtual void LoadSound(const std::size_t key, const char* path) = 0;
    virtual void UnloadSound(const std::size_t key) = 0;
    virtual bool HasSound(const std::size_t key) const = 0;
    virtual void PlaySound(const std::size_t key, int channel = -1, int loops = 0) const = 0;
    virtual void PauseSound(int channel = -1) const = 0;
    virtual void ResumeSound(int channel = -1) const = 0;
    virtual void StopSound(int channel = -1) const = 0;
    virtual void FadeInSound(const std::size_t key, int channel = -1, int loops = 0, milliseconds time = seconds{ 1 }) const = 0;
    virtual void FadeOutSound(int channel = -1, milliseconds time = seconds{ 1 }) const = 0;
    virtual bool IsSoundPlaying(int channel = -1) const = 0;
    virtual bool IsSoundPaused(int channel = -1) const = 0;
    virtual bool IsSoundFading(int channel = -1) const = 0;

    virtual void LoadMusic(const std::size_t key, const char* path) = 0;
    virtual void UnloadMusic(const std::size_t key) = 0;
    virtual bool HasMusic(const std::size_t key) const = 0;
    virtual void PlayMusic(const std::size_t key, int loops = 0) const = 0;
    virtual void PauseMusic() const = 0;
    virtual void ResumeMusic() const = 0;
    virtual void StopMusic() const = 0;
    virtual void FadeInMusic(const std::size_t key, int loops = 0, milliseconds time = seconds{ 1 }) const = 0;
    virtual void FadeOutMusic(milliseconds time = seconds{ 1 }) const = 0;
    virtual bool IsMusicPlaying() const = 0;
    virtual bool IsMusicPaused() const = 0;
    virtual bool IsMusicFading() const = 0;
};

} // namespace interface

namespace internal {

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
    virtual void UnloadSound(const std::size_t key) override;
    virtual bool HasSound(const std::size_t key) const override;
    virtual void PlaySound(const std::size_t key, int channel = -1, int loops = 0) const override;
    virtual void PauseSound(int channel = -1) const override;
    virtual void ResumeSound(int channel = -1) const override;
    virtual void StopSound(int channel = -1) const override;
    virtual void FadeInSound(const std::size_t key, int channel = -1, int loops = 0, milliseconds time = seconds{ 1 }) const override;
    virtual void FadeOutSound(int channel = -1, milliseconds time = seconds{ 1 }) const override;
    virtual bool IsSoundPlaying(int channel = -1) const override;
    virtual bool IsSoundPaused(int channel = -1) const override;
    virtual bool IsSoundFading(int channel = -1) const override;
    
    virtual void LoadMusic(const std::size_t key, const char* path) override;
    virtual void UnloadMusic(const std::size_t key) override;
    virtual bool HasMusic(const std::size_t key) const override;
    virtual void PlayMusic(const std::size_t key, int loops = 0) const override;
    virtual void ResumeMusic() const override;
    virtual void PauseMusic() const override;
    virtual void StopMusic() const override;
    virtual void FadeInMusic(const std::size_t key, int loops = 0, milliseconds time = seconds{ 1 }) const override;
    virtual void FadeOutMusic(milliseconds time = seconds{ 1 }) const override;
    virtual bool IsMusicPlaying() const override;
    virtual bool IsMusicPaused() const override;
    virtual bool IsMusicFading() const override;

private:
    void SetSound(const std::size_t key, Mix_Chunk* sound);
    void SetMusic(const std::size_t key, Mix_Music* music);
    Mix_Chunk* GetSound(const std::size_t key) const;
    Mix_Music* GetMusic(const std::size_t key) const;

	std::unordered_map<std::size_t, std::unique_ptr<Mix_Chunk, SDLSoundDeleter>> sound_map_;
	std::unordered_map<std::size_t, std::unique_ptr<Mix_Music, SDLMusicDeleter>> music_map_;
};

SDLSoundManager& GetSDLSoundManager();

} // namespace internal

namespace services {

interfaces::SoundManager& GetSoundManager();

} // namespace services

} // namespace ptgn