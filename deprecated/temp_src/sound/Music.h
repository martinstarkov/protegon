#pragma once

#include "utility/Time.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;

namespace ptgn {

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

	bool IsValid() const { return music_ != nullptr; }
	operator Mix_Music*() const { return music_; }
private:
	Mix_Music* music_{ nullptr };
};

} // namespace ptgn