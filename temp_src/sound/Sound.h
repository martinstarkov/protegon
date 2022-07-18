#pragma once

#include "utility/Time.h"

struct Mix_Chunk;

namespace ptgn {

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

	bool IsValid() const { return chunk_ != nullptr; }
	operator Mix_Chunk* () const { return chunk_; }
private:
	Mix_Chunk* chunk_{ nullptr };
};

} // namespace ptgn