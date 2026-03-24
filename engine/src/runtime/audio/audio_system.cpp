#include "runtime/audio/audio_system.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <algorithm>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "ecs/ecs.h"
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/track.h"

namespace ptgn {

namespace impl {

void MIX_AudioDeleter::operator()(MIX_Audio* audio) const {
	MIX_DestroyAudio(audio);
}

} // namespace impl

AudioSystem::AudioSystem(AssetManager& assets) : assets_{ assets } {
	mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);

	PTGN_ASSERT(mixer_, SDL_GetError());
}

AudioSystem::~AudioSystem() noexcept {
	tracks_.clear();

	MIX_DestroyMixer(mixer_);
}

void AudioSystem::Play(AudioOrKey audio, float volume, int loops, float frequency_ratio) {
	auto resolved_audio{ audio.Get(assets_) };

	MIX_Audio* mix_audio = resolved_audio.GetEntity().Get<std::shared_ptr<MIX_Audio>>().get();

	PTGN_ASSERT(mix_audio);

	auto id{ HashAsset(audio) };

	impl::Track track{ id, mixer_, mix_audio, loops };

	volume = std::clamp(volume, kMinVolume, kMaxVolume);
	MIX_SetTrackGain(track.Get(), volume);

	frequency_ratio = std::clamp(frequency_ratio, kMinFrequencyRatio, kMaxFrequencyRatio);
	MIX_SetTrackFrequencyRatio(track.Get(), frequency_ratio);

	tracks_.emplace_back(std::move(track));
}

void AudioSystem::Stop(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	std::erase_if(tracks_, [id](auto& track) {
		if (track.GetId() == id) {
			if (auto* t = track.Get()) {
				MIX_StopTrack(t, 0);
			}
			return true;
		}
		return false;
	});
}

void AudioSystem::Pause(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return;
	}

	MIX_PauseTrack(it->Get()); // prefer the dedicated API if available
	// (If you're using MIX_SetTrackPaused, keep it; Pause semantics do not fire stopped callback.)
}

void AudioSystem::Resume(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return;
	}

	MIX_ResumeTrack(it->Get()); // prefer the dedicated API if available
}

void AudioSystem::TogglePause(AudioOrKey audio) {
	if (IsPaused(audio)) {
		Resume(audio);
	} else {
		Pause(audio);
	}
}

bool AudioSystem::IsPaused(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return false;
	}

	MIX_Track* track = it->Get();
	if (!track) {
		return false;
	}

	return MIX_TrackPaused(track);
}

bool AudioSystem::IsPlaying(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return false;
	}

	MIX_Track* track = it->Get();
	if (!track) {
		return false;
	}

	return MIX_TrackPlaying(track);
}

void AudioSystem::SetVolume(AudioOrKey audio, float volume) {
	auto id{ HashAsset(audio) };

	float clamped{ std::clamp(volume, kMinVolume, kMaxVolume) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return;
	}

	MIX_Track* track = it->Get();
	if (!track) {
		return;
	}

	MIX_SetTrackGain(track, clamped);
}

float AudioSystem::GetVolume(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it == tracks_.end()) {
		return kMinVolume;
	}

	MIX_Track* track = it->Get();
	if (!track) {
		return kMinVolume;
	}

	return MIX_GetTrackGain(track);
}

void AudioSystem::ToggleVolume(AudioOrKey audio, float new_volume) {
	PTGN_ASSERT(new_volume >= kMinVolume && new_volume <= kMaxVolume);

	float current = GetVolume(audio);

	if (current > kMinVolume) {
		SetVolume(audio, kMinVolume);
	} else {
		SetVolume(audio, new_volume);
	}
}

void AudioSystem::SetVolume(float volume) {
	const float clamped = std::clamp(volume, kMinVolume, kMaxVolume);
	MIX_SetMixerGain(mixer_, clamped);
}

float AudioSystem::GetVolume() {
	return MIX_GetMixerGain(mixer_);
}

void AudioSystem::ToggleVolume(float new_volume) {
	PTGN_ASSERT(new_volume >= kMinVolume && new_volume <= kMaxVolume);

	float current = GetVolume();

	if (current > kMinVolume) {
		SetVolume(kMinVolume);
	} else {
		SetVolume(new_volume);
	}
}

void AudioSystem::PauseAll() const {
	for (const auto& track : tracks_) {
		MIX_PauseTrack(track.Get());
	}
}

void AudioSystem::ResumeAll() const {
	for (const auto& track : tracks_) {
		MIX_ResumeTrack(track.Get());
	}
}

void AudioSystem::StopAll() {
	tracks_.clear();
}

bool AudioSystem::IsAnyPlaying() const {
	for (const auto& track : tracks_) {
		MIX_Track* raw = track.Get();
		if (!raw) {
			continue;
		}

		if (MIX_TrackPlaying(raw)) {
			return true;
		}
	}

	return false;
}

void AudioSystem::Update() {
	std::erase_if(tracks_, [](const auto& track) {
		MIX_Track* raw = track.Get();
		return !raw || !MIX_TrackPlaying(raw);
	});
}

std::shared_ptr<MIX_Audio> AudioSystem::CreateAudio(const path& audio_path) const {
	PTGN_ASSERT(
		FileExists(audio_path), "Cannot create audio from invalid path: ", audio_path.string()
	);

	PTGN_ASSERT(mixer_, "Cannot load audio when SDL_mixer has not been created");

	auto mix_audio = MIX_LoadAudio(mixer_, audio_path.string().c_str(), true);

	PTGN_ASSERT(mix_audio, SDL_GetError());

	return std::shared_ptr<MIX_Audio>{ mix_audio, impl::MIX_AudioDeleter{} };
}

} // namespace ptgn