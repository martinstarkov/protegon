#pragma once

#include "utility/Time.h"

namespace ptgn {

namespace sound {

void Load(const char* sound_key, const char* sound_path);

void Play(const char* sound_key, int channel, int loops);

void FadeIn(const char* sound_key, int channel, int loops, milliseconds time);

bool Exists(const char* sound_key);

void Pause(int channel);

void Resume(int channel);

void Stop(int channel);

void FadeOut(int channel, milliseconds time);

bool IsPlaying(int channel);

bool IsPaused(int channel);

bool IsFading(int channel);

} // namespace sound

} // namespace ptgn