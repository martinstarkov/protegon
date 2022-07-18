#pragma once

#include "manager/ResourceManager.h"
#include "sound/Music.h"
#include "utility/Time.h"

namespace ptgn {

class MusicManager : public manager::ResourceManager<Music> {
public:
	static void Pause();
	static void Resume();
	static void Stop();
	static void FadeOut(milliseconds time);
	static bool IsPlaying();
	static bool IsPaused();
	static bool IsFading();
};

} // namespace ptgn