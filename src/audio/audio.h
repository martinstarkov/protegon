#pragma once

#include "core/manager.h"
#include "utility/file.h"
#include "utility/handle.h"
#include "utility/time.h"

struct _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn {

constexpr int max_volume{ 128 };

class Music : public Handle<Mix_Music> {
public:
	Music() = default;

	// @param music_path The file path to the music.
	explicit Music(const path& music_path);

	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void Play(int loops = -1);

	// @param fade_time How long to fade the music in for.
	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void FadeIn(milliseconds fade_time, int loops = -1);
};

class Sound : public Handle<Mix_Chunk> {
public:
	Sound() = default;

	// @param sound_path The file path to the sound.
	explicit Sound(const path& sound_path);

	// TODO: Potentially store the channel to enable commands like IsPlaying(). Although this might
	// be problematic if multiple sounds play on the same channel.

	// @param channel The channel on which to play the sound on, -1 plays on the first available channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void Play(int channel = -1, int loops = 0);

	// @param fade_time Time over which to fade the sound in.
	// @param channel The channel on which to play the sound on, -1 plays on the first available channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void FadeIn(milliseconds fade_time, int channel = -1, int loops = 0);

	// Set volume of the sound. Volume range: [0, 128].
	void SetVolume(int volume);

	// @return Volume of the sound. Volume range: [0, 128].
	[[nodiscard]] int GetVolume() const;

	// Toggles the sound volume between 0 and new_volume.
	// @param new_volume When toggle unmutes, it will set the new volume of the sound to this value in range [0, 128].
	void ToggleMute(int new_volume = max_volume);

	// Sets sound volume to 0.
	void Mute();

	// @param new_volume Volume to set the sound after unmuting in range [0, 128].
	void Unmute(int new_volume = max_volume);
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

	// Pause the currently playing music.
	void Pause() const;

	// Resume the currently playing music.
	void Resume() const;

	// Toggles the pause state of the music.
	void TogglePause() const;

	// @return The current music track volume in range [0, 128].
	[[nodiscard]] int GetVolume() const;

	// @param volume Volume of the music in range [0, 128].
	void SetVolume(int volume) const;

	// Toggles the volume between 0 and new_volume.
	// @param new_volume When toggle unmutes, it will set the new volume of the music to this value in range [0, 128].
	void ToggleMute(int new_volume = max_volume) const;

	// Sets music volume to 0.
	void Mute() const;

	// @param new_volume Volume to set the music after unmuting in range [0, 128].
	void Unmute(int new_volume = max_volume) const;

	// Stop the currently playing music.
	void Stop() const;

	// @param fade_time Time over which to fade the music out.
	void FadeOut(milliseconds fade_time) const;

	// @return True if any music is currently playing, false otherwise.
	[[nodiscard]] bool IsPlaying() const;

	// @return True if the currently playing music is paused, false otherwise.
	[[nodiscard]] bool IsPaused() const;

	// @return True if the currently playing music is fading in OR out, false otherwise.
	[[nodiscard]] bool IsFading() const;

	// Stops the currently playing music in addition to clearing the music manager.
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

	// Stops the sound playing on the specified channel, -1 stops all sound channels.
	void Stop(int channel) const;

	// Resumes the sound playing on the specified channel, -1 resumes all paused sound channels.
	void Resume(int channel) const;

	// Pauses the sound playing on the specified channel, -1 pauses all sound channels.
	void Pause(int channel) const;

	// Toggles the pause state of the channel.
	void TogglePause(int channel) const;

	// @param fade_time Time over which to fade the sound in.
	// @param channel The channel on which to play the sound on, -1 plays on the first available channel.
	void FadeOut(milliseconds fade_time, int channel) const;

	// Set volume of the channel. 
	// @param channel The channel for which the volume is set, -1 sets the volume for all sound channels.
	// @param volume Volume of the sound channel. Volume range: [0, 128].
	void SetVolume(int channel, int volume);

	// @param channel The channel for which to query the volume, -1 gets the average of all sound channels.
	// @return Volume of the sound. Volume range: [0, 128].
	[[nodiscard]] int GetVolume(int channel) const;

	// @return True if the sound channel is playing, -1 to check if any channel is playing.
	[[nodiscard]] bool IsPlaying(int channel) const;

	// @return True if the sound channel is paused, -1 to check if any channel is paused.
	[[nodiscard]] bool IsPaused(int channel) const;

	// @return True if the sound channel is fading in or out, -1 to check if any channel is playing.
	[[nodiscard]] bool IsFading(int channel) const;

	// Stops all sound channels in addition to clearing the sound manager.
	void Reset();
};

} // namespace impl

} // namespace ptgn