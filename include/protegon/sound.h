#pragma once

#include "file.h"
#include "handle.h"
#include "time.h"

struct _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn {

class Music : public Handle<Mix_Music> {
public:
	Music() = default;
	Music(const path& music_path);

	void Play(int loops) const;
	void FadeIn(int loops, milliseconds time) const;
};

class Sound : public Handle<Mix_Chunk> {
public:
	Sound() = default;
	Sound(const path& sound_path);

	void Play(int channel, int loops = 0) const;
	void FadeIn(int channel, int loops, milliseconds time) const;
};

} // namespace ptgn