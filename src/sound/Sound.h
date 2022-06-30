#pragma once

#include "utils/Timer.h"

struct Mix_Chunk;

namespace ptgn {

namespace internal {

struct Sound {
public:
	Sound() = delete;
	/*
	* @param sound_path Path to sound file.
	*/
	Sound(const char* sound_path);
	~Sound();

	void Play(int channel, int loops) const;
	void Pause(int channel) const;
	void Resume(int channel) const;
	void Stop(int channel) const;
	void FadeOut(int channel, milliseconds time) const;
	void FadeIn(int channel, int loops, milliseconds time) const;
	bool IsPlaying(int channel) const;
	bool IsPaused(int channel) const;
	bool IsFading(int channel) const;

	operator Mix_Chunk*() const;
private:
	Mix_Chunk* chunk_;
};

} // namespace internal

} // namespace ptgn