#pragma once

#include "managers/SDLManager.h"
#include "sound/Music.h"
#include "sound/Sound.h"
#include "utility/Time.h"

namespace ptgn {

namespace managers {

class SoundManager : public SDLManager<Sound> {
public:
	static void Pause(int channel);
	static void Resume(int channel);
	static void Stop(int channel);
	static void FadeOut(int channel, milliseconds time);
	static bool IsPlaying(int channel);
	static bool IsPaused(int channel);
	static bool IsFading(int channel);
};

class MusicManager : public SDLManager<Music> {
public:
	static void Pause();
	static void Resume();
	static void Stop();
	static void FadeOut(milliseconds time);
	static bool IsPlaying();
	static bool IsPaused();
	static bool IsFading();
};

} // namespace managers

} // namespace ptgn