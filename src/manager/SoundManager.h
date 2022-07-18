#pragma once

#include "manager/ResourceManager.h"
#include "sound/Sound.h"
#include "utility/Time.h"

namespace ptgn {

class SoundManager : public manager::ResourceManager<Sound> {
public:
	static void Pause(int channel);
	static void Resume(int channel);
	static void Stop(int channel);
	static void FadeOut(int channel, milliseconds time);
	static bool IsPlaying(int channel);
	static bool IsPaused(int channel);
	static bool IsFading(int channel);
};

} // namespace ptgn