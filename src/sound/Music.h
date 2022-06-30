#pragma once

#include "utils/Timer.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;

namespace ptgn {

namespace internal {

struct Music {
public:
	Music() = delete;
	/*
	* @param music_path Path to music file.
	*/
	Music(const char* music_path);
	~Music();

	void Play(int loops) const;
	void Stop() const;
	void FadeIn(int loops, milliseconds time) const;
	void FadeOut(milliseconds time) const;
	void Pause() const;
	void Resume() const;
	bool IsPlaying() const;
	bool IsPaused() const;
	bool IsFading() const;

	operator Mix_Music*() const;
private:
	Mix_Music* music_;
};

} // namespace internal

} // namespace ptgn