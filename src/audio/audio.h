#pragma once

#include "core/manager.h"
#include "utility/file.h"
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

	// -1 for infinite loops.
	void Play(int loops);
	// -1 for infinite loops.
	void FadeIn(int loops, milliseconds time);
};

class Sound : public Handle<Mix_Chunk> {
public:
	Sound() = default;
	explicit Sound(const path& sound_path);

	// TODO: Potentially store the channel to enable commands like IsPlaying(). Although this might
	// be problematic if multiple sounds play on the same channel.

	// @param channel The channel on which to play the sound on.
	// @param loops Number of times to loop sound (-1 for infinite looping).
	void Play(int channel, int loops = 0);
	// @param loops Number of times to loop sound (-1 for infinite looping).
	// @param time Time over which to fade the sound in.
	void FadeIn(int channel, int loops, milliseconds time);
	// volume 0 to 128.
	void SetVolume(int volume);
	[[nodiscard]] int GetVolume();
};

namespace impl {

class MusicManager : public MapManager<Music> {
public:
	MusicManager()									 = default;
	virtual ~MusicManager()							 = default;
	MusicManager(MusicManager&&) noexcept			 = default;
	MusicManager& operator=(MusicManager&&) noexcept = default;
	MusicManager(const MusicManager&)				 = delete;
	MusicManager& operator=(const MusicManager&)	 = delete;

	void Pause() const;
	void Resume() const;
	// Returns the current music track volume from 0 to 128 (MIX_MAX_VOLUME).
	[[nodiscard]] int GetVolume() const;
	// Volume can be set from 0 to 128 (MIX_MAX_VOLUME).
	void SetVolume(int new_volume) const;
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Toggle(int optional_new_volume = -1) const;
	// Sets volume to 0.
	void Mute() const;
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Unmute(int optional_new_volume = -1) const;
	void Stop() const;
	void FadeOut(milliseconds time) const;
	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsFading() const;

	void Reset();
};

class SoundManager : public MapManager<Sound> {
public:
	SoundManager()									 = default;
	virtual ~SoundManager()							 = default;
	SoundManager(SoundManager&&) noexcept			 = default;
	SoundManager& operator=(SoundManager&&) noexcept = default;
	SoundManager(const SoundManager&)				 = delete;
	SoundManager& operator=(const SoundManager&)	 = delete;

	// if, channel = -1, all channels are stopped.
	void Stop(int channel) const;
	void Resume(int channel) const;
	void FadeOut(int channel, milliseconds time) const;
	// if channel = -1, check if any channel is playing.
	[[nodiscard]] bool IsPlaying(int channel) const;

	void Reset();
};

} // namespace impl

} // namespace ptgn