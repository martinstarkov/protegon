#pragma once

#include "utils/Timer.h"

struct Mix_Chunk;

namespace ptgn {

namespace internal {

class Sound {
public:
	Sound() = delete;
	/*
	* @param sound_path Path to sound file.
	*/
	Sound(const char* sound_path);
	~Sound();

	void Play(int channel, int loops) const;
	void FadeIn(int channel, int loops, milliseconds time) const;

	operator Mix_Chunk*() const;
private:
	Mix_Chunk* chunk_{ nullptr };
};

} // namespace internal

} // namespace ptgn