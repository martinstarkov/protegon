#pragma once

#include "protegon/file.h"
#include "utility/handle.h"
#include "utility/time.h"

struct _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn {

class Music : public Handle<Mix_Music> {
public:
	Music() = default;
	explicit Music(const path& music_path);

	void Play(int loops);
	void FadeIn(int loops, milliseconds time);
};

class Sound : public Handle<Mix_Chunk> {
public:
	Sound() = default;
	explicit Sound(const path& sound_path);

	// @param channel The channel on which to play the sound on.
	// @param loops Number of times to loop sound (-1 for infinite looping).
	void Play(int channel, int loops = 0);
	// @param loops Number of times to loop sound (-1 for infinite looping).
	// @param time Time over which to fade the sound in.
	void FadeIn(int channel, int loops, milliseconds time);
};

} // namespace ptgn