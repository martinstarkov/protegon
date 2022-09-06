#pragma once

#include <memory> // std::shared_ptr

#include "time.h"

class _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn {

class Music {
public:
	Music(const char* music_path);
	~Music() = default;
	Music(const Music&) = default;
	Music& operator=(const Music&) = default;
	Music(Music&&) = default;
	Music& operator=(Music&&) = default;

	void Play(int loops) const;
	void FadeIn(int loops, milliseconds time) const;
	bool IsValid() const;
private:
	Music() = default;
	std::shared_ptr<Mix_Music> music_{ nullptr };
};

class Sound {
public:
	Sound(const char* sound_path);
	~Sound() = default;
	Sound(const Sound&) = default;
	Sound& operator=(const Sound&) = default;
	Sound(Sound&&) = default;
	Sound& operator=(Sound&&) = default;

	void Play(int channel, int loops) const;
	void FadeIn(int channel, int loops, milliseconds time) const;
	bool IsValid() const;
private:
	Sound() = default;
	std::shared_ptr<Mix_Chunk> chunk_{ nullptr };
};

} // namespace ptgn