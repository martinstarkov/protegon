#pragma once

#include <memory>
#include <string_view>
#include <unordered_map>

#include "utility/file.h"
#include "utility/time.h"

struct _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

namespace ptgn::impl {

constexpr int max_volume{ 128 };

struct Mix_MusicDeleter {
	void operator()(Mix_Music* music) const;
};

struct Mix_ChunkDeleter {
	void operator()(Mix_Chunk* sound) const;
};

class MusicManager {
public:
	// @param filepath The file path to the music.
	void Load(std::string_view key, const path& filepath);

	void Unload(std::string_view key);

	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void Play(std::string_view key, int loops = -1);

	// @param fade_time How long to fade the music in for.
	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void FadeIn(std::string_view key, milliseconds fade_time, int loops = -1);

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
	// @param new_volume When toggle unmutes, it will set the new volume of the music to this value
	// in range [0, 128].
	void ToggleVolume(int new_volume = max_volume) const;

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

	void Clear();

private:
	using Music = std::unique_ptr<Mix_Music, Mix_MusicDeleter>;

	[[nodiscard]] static Music LoadFromFile(const path& filepath);

	[[nodiscard]] Mix_Music* Get(std::size_t key) const;

	[[nodiscard]] bool Has(std::size_t key) const;

	std::unordered_map<std::size_t, Music> music_;
};

class SoundManager {
public:
	// @param filepath The file path to the sound.
	void Load(std::string_view key, const path& filepath);

	void Unload(std::string_view key);

	// @param channel The channel on which to play the sound on, -1 plays on the first available
	// channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void Play(std::string_view key, int channel = -1, int loops = 0);

	// @param fade_time Time over which to fade the sound in.
	// @param channel The channel on which to play the sound on, -1 plays on the first available
	// channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void FadeIn(std::string_view key, milliseconds fade_time, int channel = -1, int loops = 0);

	// Set volume of the sound. Volume range: [0, 128].
	void SetVolume(std::string_view key, int volume);

	// @return Volume of the sound. Volume range: [0, 128].
	[[nodiscard]] int GetVolume(std::string_view key) const;

	// Toggles the sound volume between 0 and new_volume.
	// @param new_volume When toggle unmutes, it will set the new volume of the sound to this value
	// in range [0, 128].
	void ToggleVolume(std::string_view key, int new_volume = max_volume);

	// Stops the sound playing on the specified channel, -1 stops all sound channels.
	void Stop(int channel) const;

	// Resumes the sound playing on the specified channel, -1 resumes all paused sound channels.
	void Resume(int channel) const;

	// Pauses the sound playing on the specified channel, -1 pauses all sound channels.
	void Pause(int channel) const;

	// Toggles the pause state of the channel.
	void TogglePause(int channel) const;

	// @param fade_time Time over which to fade the sound in.
	// @param channel The channel on which to play the sound on, -1 plays on the first available
	// channel.
	void FadeOut(milliseconds fade_time, int channel) const;

	// Set volume of the channel.
	// @param channel The channel for which the volume is set, -1 sets the volume for all sound
	// channels.
	// @param volume Volume of the sound channel. Volume range: [0, 128].
	void SetVolume(int channel, int volume);

	// @param channel The channel for which to query the volume, -1 gets the average of all sound
	// channels.
	// @return Volume of the sound. Volume range: [0, 128].
	[[nodiscard]] int GetVolume(int channel) const;

	// @return True if the sound channel is playing, -1 to check if any channel is playing.
	[[nodiscard]] bool IsPlaying(int channel) const;

	// @return True if the sound channel is paused, -1 to check if any channel is paused.
	[[nodiscard]] bool IsPaused(int channel) const;

	// @return True if the sound channel is fading in or out, -1 to check if any channel is playing.
	[[nodiscard]] bool IsFading(int channel) const;

	void Clear();

private:
	using Sound = std::unique_ptr<Mix_Chunk, Mix_ChunkDeleter>;

	[[nodiscard]] static Sound LoadFromFile(const path& filepath);

	[[nodiscard]] Mix_Chunk* Get(std::size_t key) const;

	[[nodiscard]] bool Has(std::size_t key) const;

	std::unordered_map<std::size_t, Sound> sounds_;
};

} // namespace ptgn::impl