#pragma once

#include <memory>
#include <vector>

#include "core/util/file.h"
#include "runtime/asset/asset.h"
#include "runtime/audio/track.h"

struct MIX_Audio;
struct MIX_Mixer;
struct MIX_Track;

namespace ptgn {

inline constexpr float kMinVolume{ 0.0f };
inline constexpr float kMaxVolume{ 5.0f };
inline constexpr float kMinFrequencyRatio{ 0.01f };
inline constexpr float kMaxFrequencyRatio{ 100.0f };

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
	float GetVolume();

	/// @brief Toggles the master volume between kMinVolume and new_volume.
	/// @param new_volume When toggle unmutes, it will set the new master volume to this value
	/// in range [kMinVolume, kMaxVolume].
	void ToggleVolume(float new_volume);

	/// @brief Stops all audio tracks.
	void StopAll();

	/// @brief Pauses all audio tracks.
	void PauseAll() const;

	/// @brief Resumes all audio tracks.
	void ResumeAll() const;

	/// @return True if any audio track is playing.
	[[nodiscard]] bool IsAnyPlaying() const;

	/// @param loops The number of loops to play the audio for, -1 for infinite looping.
	/// @param volume Volume of the specific audio in range [kMinVolume, kMaxVolume].
	/// @param frequency_ratio The frequency ratio is used to adjust the rate at which audio data is
	/// consumed. Range: [0.01, 100.0]. Changing this effectively modifies the speed and pitch of
	/// the track's audio. A value greater than 1.0f will play the audio faster, and at a higher
	/// pitch. A value less than 1.0f will play the audio slower, and at a lower pitch. 1.0f is
	/// normal speed.
	void Play(AudioOrKey audio, float volume = 1.0f, int loops = 0, float frequency_ratio = 1.0f);

	/// @brief Stop the audio.
	void Stop(AudioOrKey audio);

	/// @brief Pauses the audio.
	void Pause(AudioOrKey audio);

	/// @brief Resumes the audio.
	void Resume(AudioOrKey audio);

	/// @brief Toggles the pause state of the audio.
	void TogglePause(AudioOrKey audio);

	/// @brief Only sets the volume of the specific audio if it's currently playing; otherwise, does
	/// nothing.
	/// @param volume Volume of the specific audio in range [kMinVolume, kMaxVolume].
	void SetVolume(AudioOrKey audio, float volume);

	/// @brief Only gets the volume of the specific audio if it's currently playing; otherwise,
	/// returns 0
	/// @return Volume of the specific audio in range [kMinVolume, kMaxVolume].
	float GetVolume(AudioOrKey audio);

	/// @brief Toggles the volume between kMinVolume and new_volume.
	/// @param new_volume When toggle unmutes, it will set the new volume of the audio to this value
	/// in range [kMinVolume, kMaxVolume].
	void ToggleVolume(AudioOrKey audio, float new_volume = 1.0f);

	/// @return True if the audio is currently, false otherwise.
	[[nodiscard]] bool IsPlaying(AudioOrKey audio);

	/// @return True if the audio is currently paused, false otherwise.
	[[nodiscard]] bool IsPaused(AudioOrKey audio);

	///// @return True if the audio is currently fading in OR out, false otherwise.
	//[[nodiscard]] bool IsFading(AudioOrKey audio);
	///// @param fade_time How long to fade the audio in for.
	///// @param loops The number of loops to play the audio for, -1 for infinite looping.
	// void FadeIn(AudioOrKey audio, milliseconds fade_time, int loops =
	// -1);
	///// @param fade_time Time over which to fade the audio out.
	// void FadeOut(AudioOrKey audio, milliseconds fade_time);

private:
	friend class AssetManager;
	friend class Application;

	void Update();

	std::shared_ptr<MIX_Audio> CreateAudio(const path& audio_path) const;

	AssetManager& assets_;

	MIX_Mixer* mixer_{ nullptr };

	std::vector<impl::Track> tracks_;
};

} // namespace ptgn