#include "runtime/audio/audio_system.h"

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <filesystem>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include "core/assert.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include "core/util/hash.h"
#include "ecs/ecs.h"
#include "runtime/asset/asset_manager.h"

namespace ptgn {

namespace impl {

void MIX_AudioDeleter::operator()(MIX_Audio* audio) const {
	MIX_DestroyAudio(audio);
}

} // namespace impl

AudioSystem::AudioSystem(AssetManager& assets) : assets_{ assets } {
	mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);

	PTGN_ASSERT(mixer_, SDL_GetError());

	// TODO: Fix.
	// MIX_SetTrackStoppedCallback(mixer_, &AudioSystem::OnTrackStopped, this);
}

AudioSystem::~AudioSystem() noexcept {
	std::scoped_lock lock(mutex_);
	tracks_.clear(); // RAII for Track should destroy underlying MIX_Track.
	pending_removals_.clear();

	MIX_DestroyMixer(mixer_);
}

void AudioSystem::Play(std::string_view key, int loops) {
	std::size_t id = Hash(key);

	Stop(key); // replace if present

	auto audio = assets_.GetAudio(key);
	PTGN_ASSERT(
		audio.has_value(), "Cannot play audio '", key,
		"' which has not been loaded into the asset manager"
	);
	MIX_Audio* mix_audio = audio->entity_.Get<std::shared_ptr<MIX_Audio>>().get();

	PTGN_ASSERT(mix_audio);

	impl::Track track{ mixer_, mix_audio, loops };

	// Attach per-track stopped callback (per SDL3_mixer API).
	// Track owns callback userdata for lifetime safety.
	track.SetStoppedCallback(&AudioSystem::OnTrackStopped, this, id);

	std::scoped_lock lock(mutex_);
	tracks_.emplace(id, std::move(track));
}

void AudioSystem::Stop(std::string_view key) {
	std::size_t id{ Hash(key) };

	std::scoped_lock lock(mutex_);
	tracks_.erase(id);
	// Note: MIX_DestroyTrack() does NOT call the stopped callback.
}

void AudioSystem::Pause(std::string_view key) {
	std::size_t id = Hash(key);

	std::scoped_lock lock(mutex_);
	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return;
	}

	MIX_PauseTrack(it->second.Get()); // prefer the dedicated API if available
	// (If you're using MIX_SetTrackPaused, keep it; Pause semantics do not fire stopped callback.)
}

void AudioSystem::Resume(std::string_view key) {
	std::size_t id = Hash(key);

	std::scoped_lock lock(mutex_);

	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return;
	}

	MIX_ResumeTrack(it->second.Get()); // prefer the dedicated API if available
}

void AudioSystem::TogglePause(std::string_view key) {
	if (IsPaused(key)) {
		Resume(key);
	} else {
		Pause(key);
	}
}

bool AudioSystem::IsPaused(std::string_view key) {
	std::size_t id = Hash(key);

	std::scoped_lock lock(mutex_);

	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return false;
	}

	MIX_Track* track = it->second.Get();
	if (!track) {
		return false;
	}

	return MIX_TrackPaused(track);
}

bool AudioSystem::IsPlaying(std::string_view key) {
	std::size_t id = Hash(key);

	std::scoped_lock lock(mutex_);

	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return false;
	}

	MIX_Track* track = it->second.Get();
	if (!track) {
		return false;
	}

	return MIX_TrackPlaying(track);
}

void AudioSystem::SetVolume(std::string_view key, float volume) {
	std::size_t id = Hash(key);

	const float clamped = std::clamp(volume, min_volume, max_volume);

	std::scoped_lock lock(mutex_);

	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return;
	}

	MIX_Track* track = it->second.Get();
	if (!track) {
		return;
	}

	MIX_SetTrackGain(track, clamped);
}

float AudioSystem::GetVolume(std::string_view key) {
	std::size_t id = Hash(key);

	std::scoped_lock lock(mutex_);

	auto it = tracks_.find(id);
	if (it == tracks_.end()) {
		return min_volume;
	}

	MIX_Track* track = it->second.Get();
	if (!track) {
		return min_volume;
	}

	return MIX_GetTrackGain(track);
}

void AudioSystem::ToggleVolume(std::string_view key, float new_volume) {
	PTGN_ASSERT(new_volume >= min_volume && new_volume <= max_volume);

	float current = GetVolume(key);

	if (current > min_volume) {
		SetVolume(key, min_volume);
	} else {
		SetVolume(key, new_volume);
	}
}

void AudioSystem::OnTrackStopped(void* userdata, [[maybe_unused]] MIX_Track* track) {
	auto* data = static_cast<impl::CallbackData*>(userdata);
	auto* self = data->self;

	// Called from the mixer thread; don't touch tracks_ here.
	std::scoped_lock lock(self->mutex_);
	self->pending_removals_.push_back(data->id);
}

void AudioSystem::SetVolume(float volume) {
	const float clamped = std::clamp(volume, min_volume, max_volume);
	MIX_SetMixerGain(mixer_, clamped);
}

float AudioSystem::GetVolume() {
	return MIX_GetMixerGain(mixer_);
}

void AudioSystem::ToggleVolume(float new_volume) {
	PTGN_ASSERT(new_volume >= min_volume && new_volume <= max_volume);

	float current = GetVolume();

	if (current > min_volume) {
		SetVolume(min_volume);
	} else {
		SetVolume(new_volume);
	}
}

void AudioSystem::PauseAll() {
	std::scoped_lock lock(mutex_);
	for (auto& [id, track] : tracks_) {
		MIX_PauseTrack(track.Get());
	}
}

void AudioSystem::ResumeAll() {
	std::scoped_lock lock(mutex_);
	for (auto& [id, track] : tracks_) {
		MIX_ResumeTrack(track.Get());
	}
}

void AudioSystem::StopAll() {
	std::scoped_lock lock(mutex_);

	tracks_.clear();
	pending_removals_.clear();
}

bool AudioSystem::IsAnyPlaying() {
	std::scoped_lock lock(mutex_);

	for (auto& [id, track] : tracks_) {
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
	std::vector<std::size_t> to_remove;

	{
		std::scoped_lock lock(mutex_);
		to_remove.swap(pending_removals_);
	}

	if (to_remove.empty()) {
		return;
	}

	std::scoped_lock lock(mutex_);
	for (std::size_t id : to_remove) {
		tracks_.erase(id); // RAII destroys track here
	}
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