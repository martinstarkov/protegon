#include "runtime/audio/audio_system.h"

#include <miniaudio.h>

#include <algorithm>
#include <memory>
#include <new>
#include <optional>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/entity_handle.h"
#include "core/util/file.h"
#include <ecs/ecs.h>
#include "runtime/asset/asset.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/track.h"

namespace ptgn {

namespace impl {

void AudioEngineDeleter::operator()(ma_engine* engine) const noexcept {
	if (engine) {
		ma_engine_uninit(engine);
		delete engine;
	}
}

} // namespace impl

AudioSystem::AudioSystem(AssetManager& assets) : assets_{ assets } {
	engine_ = std::unique_ptr<ma_engine, impl::AudioEngineDeleter>{ new ma_engine() };

	auto result = ma_engine_init(nullptr, engine_.get());
	PTGN_ASSERT(result == MA_SUCCESS, "ma_engine_init() failed");
}

void AudioSystem::Play(
	AudioOrKey audio, float volume, std::optional<int> loops, float frequency_ratio, bool exclusive,
	bool force_restart
) {
	if (exclusive && IsPlaying(audio)) {
		if (force_restart) {
			Stop(audio);
		} else {
			return;
		}
	}

	auto resolved_audio{ audio.Get(assets_) };
	const auto& audio_path{ resolved_audio.GetEntity().Get<impl::AudioObject>().path };

	PTGN_ASSERT(engine_);

	auto id{ HashAsset(audio) };

	impl::Track track{ id, engine_.get(), audio_path, loops };

	volume = std::clamp(volume, kMinVolume, kMaxVolume);
	track.SetVolume(volume);

	frequency_ratio = std::clamp(frequency_ratio, kMinFrequencyRatio, kMaxFrequencyRatio);
	track.SetPitch(frequency_ratio);

	tracks_.emplace_back(std::move(track));
}

void AudioSystem::Stop(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	std::erase_if(tracks_, [id](auto& track) { return track.GetId() == id; });
}

void AudioSystem::Pause(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end()) {
		it->Pause();
	}
}

void AudioSystem::Resume(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end()) {
		it->Resume();
	}
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

	return it != tracks_.end() && it->IsPaused();
}

bool AudioSystem::IsPlaying(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	return it != tracks_.end() && it->IsPlaying();
}

void AudioSystem::SetVolume(AudioOrKey audio, float volume) {
	auto id{ HashAsset(audio) };
	volume = std::clamp(volume, kMinVolume, kMaxVolume);

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end()) {
		it->SetVolume(volume);
	}
}

float AudioSystem::GetVolume(AudioOrKey audio) {
	auto id{ HashAsset(audio) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	return it != tracks_.end() ? it->GetVolume() : kMinVolume;
}

void AudioSystem::ToggleVolume(AudioOrKey audio, float new_volume) {
	new_volume = std::clamp(new_volume, kMinVolume, kMaxVolume);

	if (float current{ GetVolume(audio) }; current == kMinVolume) {
		SetVolume(audio, new_volume);
	} else {
		SetVolume(audio, kMinVolume);
	}
}

void AudioSystem::SetVolume(float volume) const {
	volume = std::clamp(volume, kMinVolume, kMaxVolume);
	PTGN_ASSERT(engine_);
	ma_engine_set_volume(engine_.get(), volume);
}

float AudioSystem::GetVolume() const {
	PTGN_ASSERT(engine_);
	return ma_engine_get_volume(engine_.get());
}

void AudioSystem::ToggleVolume(float new_volume) {
	new_volume = std::clamp(new_volume, kMinVolume, kMaxVolume);

	if (float current{ GetVolume() }; current == kMinVolume) {
		SetVolume(new_volume);
	} else {
		SetVolume(kMinVolume);
	}
}

void AudioSystem::PauseAll() {
	for (auto& track : tracks_) {
		track.Pause();
	}
}

void AudioSystem::ResumeAll() {
	for (auto& track : tracks_) {
		track.Resume();
	}
}

void AudioSystem::StopAll() {
	tracks_.clear();
}

bool AudioSystem::IsAnyPlaying() const {
	return std::ranges::any_of(tracks_, [](const auto& track) { return track.IsPlaying(); });
}

void AudioSystem::Update() {
	std::erase_if(tracks_, [](const auto& track) { return track.IsFinished(); });
}

} // namespace ptgn