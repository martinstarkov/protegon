#pragma once

#include <memory>

#include "core/ecs/components/generic.h"
#include "core/resource/resource_manager.h"
#include "core/utils/file.h"
#include "core/utils/time.h"

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

using Music = std::unique_ptr<Mix_Music, Mix_MusicDeleter>;

class MusicManager : public ResourceManager<MusicManager, ResourceHandle, Music> {
public:
	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void Play(const ResourceHandle& key, int loops = -1) const;

	// @param fade_time How long to fade the music in for.
	// @param loops The number of loops to play the music for, -1 for infinite looping.
	void FadeIn(const ResourceHandle& key, milliseconds fade_time, int loops = -1) const;

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

private:
	friend ParentManager;

	[[nodiscard]] static Music LoadFromFile(const path& filepath);
};

using Sound = std::unique_ptr<Mix_Chunk, Mix_ChunkDeleter>;

class SoundManager : public ResourceManager<SoundManager, ResourceHandle, Sound> {
public:
	// @param channel The channel on which to play the sound on, -1 plays on the first available
	// channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void Play(const ResourceHandle& key, int channel = -1, int loops = 0) const;

	// @param fade_time Time over which to fade the sound in.
	// @param channel The channel on which to play the sound on, -1 plays on the first available
	// channel.
	// @param loops Number of times to loop sound, -1 for infinite looping.
	void FadeIn(const ResourceHandle& key, milliseconds fade_time, int channel = -1, int loops = 0)
		const;

	// Set volume of the sound. Volume range: [0, 128].
	void SetVolume(const ResourceHandle& key, int volume) const;

	// Set volume of the channel.
	// @param channel The channel for which the volume is set, -1 sets the volume for all sound
	// channels.
	// @param volume Volume of the sound channel. Volume range: [0, 128].
	void SetVolume(int channel, int volume) const;

	// @return Volume of the sound. Volume range: [0, 128].
	[[nodiscard]] int GetVolume(const ResourceHandle& key) const;

	// Toggles the sound volume between 0 and new_volume.
	// @param new_volume When toggle unmutes, it will set the new volume of the sound to this value
	// in range [0, 128].
	void ToggleVolume(const ResourceHandle& key, int new_volume = max_volume) const;

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

private:
	friend ParentManager;

	[[nodiscard]] static Sound LoadFromFile(const path& filepath);
};

} // namespace ptgn::impl