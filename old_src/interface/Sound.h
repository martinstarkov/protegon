#pragma once

#include "utils/Timer.h"

namespace ptgn {

namespace sound {

// Loads a sound with the given key and path into the sound manager.
void Load(const char* key, const char* path);

// Unloads a sound with the given key from the sound manager.
void Unload(const char* key);

// Returns true if a sound with the given key has been loaded into the sound manager, false otherwise.
bool Exists(const char* key);

// Plays the sound with the given key on a channel. Multiple loops optional.
void Play(const char* key, int channel = -1, int loops = 0);

// Pauses the sound on the given channel.
void Pause(int channel = -1);

// Resumes the sound on the given channel.
void Resume(int channel = -1);

// Stops the sound on the given channel.
void Stop(int channel = -1);

// Fades the sound with the given key in on a channel for a given number of milliseconds. Multiple loops optional.
void FadeIn(const char* key, int channel = -1, int loops = 0, milliseconds fade_time = seconds{ 1 });

// Fades the given channel out for a given number of milliseconds.
void FadeOut(int channel = -1, milliseconds fade_time = seconds{ 1 });

// Returns true if the given channel is currently playing a sound, false otherwise.
bool IsPlaying(int channel = -1);

// Returns true if the given channel is currently paused, false otherwise.
bool IsPaused(int channel = -1);

// Returns true if the given channel is currently fading a sound, false otherwise.
bool IsFading(int channel = -1);

} // namespace sound

namespace music {

// Loads music with the given key and path into the sound manager.
void Load(const char* key, const char* path);

// Unloads music with the given key from the sound manager.
void Unload(const char* key);

// Returns true if music with the given key has been loaded into the sound manager, false otherwise.
bool Exists(const char* key);

// Plays music with the given key. Multiple loops optional.
void Play(const char* key, int loops = 0);

// Resumes the music from being paused.
void Resume();

// Pauses the music from being played.
void Pause();

// Stops the music from being played.
void Stop();

// Fades the music with the given key for a given number of milliseconds. Multiple loops optional.
void FadeIn(const char* key, int loops = 0, milliseconds fade_time = seconds{ 1 });

// Fades the currently playing music track out for a given number of milliseconds.
void FadeOut(milliseconds fade_time = seconds{ 1 });

// Returns true if the music is currently playing, false otherwise.
bool IsPlaying();

// Returns true if the music is currently paused, false otherwise.
bool IsPaused();

// Returns true if the music is currently fading in or out, false otherwise.
bool IsFading();

} // namespace music

} // namespace ptgn