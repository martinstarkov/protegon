#pragma once

#include <memory>
#include <mutex>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "core/util/file.h"
#include "runtime/audio/track.h"

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

namespace ptgn {

inline constexpr float kMinVolume{ 0.0f };
inline constexpr float kMaxVolume{ 5.0f };

class AssetManager;
class Application;

namespace impl {

class SDLInstance;

struct MIX_AudioDeleter {
	void operator()(MIX_Audio* audio) const;
};

} // namespace impl

class AudioSystem {
public:
	explicit AudioSystem(AssetManager& assets);
	~AudioSystem() noexcept;
	AudioSystem(const AudioSystem&)				   = delete;
	AudioSystem& operator=(const AudioSystem&)	   = delete;
	AudioSystem(AudioSystem&&) noexcept			   = delete;
	AudioSystem& operator=(AudioSystem&&) noexcept = delete;

	/// @param volume Volume of the master audio in range [kMinVolume, kMaxVolume].
	void SetVolume(float volume);

	/// @return Volume of the master audio in range [kMinVolume, kMaxVolume].
	[[nodiscard]] float GetVolume();

	/// Toggles the master volume between kMinVolume and new_volume.
	/// @param new_volume When toggle unmutes, it will set the new master volume to this value
	/// in range [kMinVolume, kMaxVolume].
	void ToggleVolume(float new_volume);

	/// Stops all audio tracks.
	void StopAll();

	/// Pauses all audio tracks.
	void PauseAll();

	/// Resumes all audio tracks.
	void ResumeAll();

	/// @return True if any audio track is playing.
	[[nodiscard]] bool IsAnyPlaying();

	/// @param loops The number of loops to play the audio for, -1 for infinite looping.
	void Play(std::string_view key, int loops = -1);

	/// Stop the audio.
	void Stop(std::string_view key);

	/// Pauses the audio.
	void Pause(std::string_view key);

	/// Resumes the audio.
	void Resume(std::string_view key);

	/// Toggles the pause state of the audio.
	void TogglePause(std::string_view key);

	/// @param volume Volume of the specific audio in range [kMinVolume, kMaxVolume].
	void SetVolume(std::string_view key, float volume);

	/// @return Volume of the specific audio in range [kMinVolume, kMaxVolume].
	[[nodiscard]] float GetVolume(std::string_view key);

	/// Toggles the volume between kMinVolume and new_volume.
	/// @param new_volume When toggle unmutes, it will set the new volume of the audio to this value
	/// in range [kMinVolume, kMaxVolume].
	void ToggleVolume(std::string_view key, float new_volume = 1.0f);

	/// @return True if the audio is currently, false otherwise.
	[[nodiscard]] bool IsPlaying(std::string_view key);

	/// @return True if the audio is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused(std::string_view key);

	// TODO: Add these functions.
	///// @return True if the audio is currently fading in OR out, false otherwise.
	//[[nodiscard]] bool IsFading(std::string_view key);
	///// @param fade_time How long to fade the audio in for.
	///// @param loops The number of loops to play the audio for, -1 for infinite looping.
	// void FadeIn(std::string_view key, milliseconds fade_time, int loops = -1);
	///// @param fade_time Time over which to fade the audio out.
	// void FadeOut(std::string_view key, milliseconds fade_time);

private:
	friend class AssetManager;
	friend class Application;

	static void OnTrackStopped(void* userdata, MIX_Track* track);

	void Update();

	std::shared_ptr<MIX_Audio> CreateAudio(const path& audio_path) const;

	AssetManager& assets_;

	MIX_Mixer* mixer_{ nullptr };

	std::unordered_map<std::size_t, impl::Track> tracks_;

	std::vector<std::size_t> pending_removals_;
	std::mutex mutex_;
};

} // namespace ptgn