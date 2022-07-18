#pragma once

#include "utils/Timer.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;

namespace ptgn {

namespace internal {

class Music {
public:
	Music() = delete;
	/*
	* @param music_path Path to music file.
	*/
	Music(const char* music_path);
	~Music();

	void Play(int loops) const;
	void FadeIn(int loops, milliseconds time) const;

	operator Mix_Music*() const;
private:
	Mix_Music* music_{ nullptr };
};

} // namespace internal

} // namespace ptgn