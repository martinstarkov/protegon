#pragma once

#include "utility/Time.h"

namespace ptgn {

namespace music {

void Load(const char* music_key, const char* music_path);

void Play(const char* music_key, int loops);

void FadeIn(const char* music_key, int loops, milliseconds time);

bool Exists(const char* music_key);

void Pause();

void Resume();

void Stop();

void FadeOut(milliseconds time);

bool IsPlaying();

bool IsPaused();

bool IsFading();

} // namespace music

} // namespace ptgn