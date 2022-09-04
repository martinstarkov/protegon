#pragma once

#include "time.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

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
	bool IsValid() const;
private:
	Mix_Music* music_{ nullptr };
};

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
	bool IsValid() const;
private:
	Mix_Chunk* chunk_{ nullptr };
};

} // namespace ptgn