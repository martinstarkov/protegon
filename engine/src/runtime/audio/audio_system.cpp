#include "runtime/audio/audio_system.h"

#include <ecs/ecs.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif

#include <miniaudio.h>

#if defined(__clang__) || defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <algorithm>
#include <cmath>
#include <memory>
#include <new>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/event/event_handler.h"
#include "core/util/entity_handle.h"
#include "core/util/hash.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/audio/audio.h"
#include "runtime/audio/audio_event.h"
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

AudioSystem::AudioSystem(AssetManager& assets, EventHandler& events) :
	assets_{ assets }, events_{ events } {
	engine_ = std::unique_ptr<ma_engine, impl::AudioEngineDeleter>{ new ma_engine() };

	auto result = ma_engine_init(nullptr, engine_.get());
	PTGN_ASSERT(result == MA_SUCCESS, "ma_engine_init() failed");
}

void AudioSystem::Play(
	const AudioKey& audio_key, float volume, std::optional<int> loops, float frequency_ratio,
	bool exclusive, bool force_restart
) {
	if (!impl::AssetAccessor{ assets_ }.Has<Audio>(audio_key)) {
		PTGN_WARN("No audio file with key ", audio_key, " found, skipping play");
		return;
	}

	if (exclusive && IsPlaying(audio_key)) {
		if (force_restart) {
			Stop(audio_key);
		} else {
			return;
		}
	}

	auto audio{ impl::AssetAccessor{ assets_ }.Get<Audio>(audio_key) };
	const auto& audio_path{ audio.GetEntity().Get<impl::AudioObject>().path };

	PTGN_ASSERT(engine_);

	auto id{ Hash(audio_key) };

	std::size_t instance_id{ next_instance_id_++ };
	impl::Track track{ id, instance_id, engine_.get(), audio_path, loops };

	volume = std::clamp(volume, kMinVolume, kMaxVolume);
	track.SetVolume(volume);

	frequency_ratio = std::clamp(frequency_ratio, kMinFrequencyRatio, kMaxFrequencyRatio);
	track.SetPitch(frequency_ratio);

	audio_keys_[id] = audio_key;
	tracks_.emplace_back(std::move(track));
	events_.Push<event::AudioStarted>(audio_key, instance_id);
}

void AudioSystem::Stop(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };
	fade_targets_.erase(id);

	std::erase_if(tracks_, [&](auto& track) {
		if (track.GetId() != id) {
			return false;
		}
		events_.Push<event::AudioStopped>(audio_key, track.GetInstanceId());
		return true;
	});
}

void AudioSystem::Pause(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end() && !it->IsPaused()) {
		it->Pause();
		events_.Push<event::AudioPaused>(audio_key, it->GetInstanceId());
	}
}

void AudioSystem::Resume(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end() && it->IsPaused()) {
		it->Resume();
		events_.Push<event::AudioResumed>(audio_key, it->GetInstanceId());
	}
}

void AudioSystem::TogglePause(const AudioKey& audio_key) {
	if (IsPaused(audio_key)) {
		Resume(audio_key);
	} else {
		Pause(audio_key);
	}
}

void AudioSystem::MarkFadeTarget(const AudioKey& audio_key, float target_volume) {
	fade_targets_[Hash(audio_key)] = std::clamp(target_volume, kMinVolume, kMaxVolume);
}

void AudioSystem::SetFrequencyRatio(const AudioKey& audio_key, float ratio) {
	std::size_t id{ Hash(audio_key) };
	float pitch{ std::clamp(ratio, kMinFrequencyRatio, kMaxFrequencyRatio) };
	for (auto& track : tracks_) {
		if (track.GetId() == id) {
			track.SetPitch(pitch);
		}
	}
}

bool AudioSystem::IsPaused(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	return it != tracks_.end() && it->IsPaused();
}

bool AudioSystem::IsPlaying(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	return it != tracks_.end() && it->IsPlaying();
}

void AudioSystem::SetVolume(const AudioKey& audio_key, float volume) {
	auto id{ Hash(audio_key) };
	volume = std::clamp(volume, kMinVolume, kMaxVolume);

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	if (it != tracks_.end() && it->GetVolume() != volume) {
		it->SetVolume(volume);
		events_.Push<event::AudioVolumeChanged>(audio_key, volume);
		auto fade{ fade_targets_.find(id) };
		if (fade != fade_targets_.end() && std::abs(fade->second - volume) < 0.0001f) {
			events_.Push<event::AudioFadeCompleted>(audio_key);
			fade_targets_.erase(fade);
		}
	}
}

float AudioSystem::GetVolume(const AudioKey& audio_key) {
	auto id{ Hash(audio_key) };

	auto it =
		std::ranges::find_if(tracks_, [id](const auto& track) { return track.GetId() == id; });

	return it != tracks_.end() ? it->GetVolume() : kMinVolume;
}

void AudioSystem::ToggleVolume(const AudioKey& audio_key, float new_volume) {
	new_volume = std::clamp(new_volume, kMinVolume, kMaxVolume);

	if (float current{ GetVolume(audio_key) }; current == kMinVolume) {
		SetVolume(audio_key, new_volume);
	} else {
		SetVolume(audio_key, kMinVolume);
	}
}

void AudioSystem::SetVolume(float volume) const {
	volume = std::clamp(volume, kMinVolume, kMaxVolume);
	PTGN_ASSERT(engine_);
	if (ma_engine_get_volume(engine_.get()) != volume) {
		ma_engine_set_volume(engine_.get(), volume);
		events_.Push<event::AudioVolumeChanged>(AudioKey{}, volume);
	}
}

float AudioSystem::GetVolume() const {
	PTGN_ASSERT(engine_);
	return ma_engine_get_volume(engine_.get());
}

void AudioSystem::ToggleVolume(float new_volume) const {
	new_volume = std::clamp(new_volume, kMinVolume, kMaxVolume);

	if (float current{ GetVolume() }; current == kMinVolume) {
		SetVolume(new_volume);
	} else {
		SetVolume(kMinVolume);
	}
}

void AudioSystem::PauseAll() {
	for (auto& track : tracks_) {
		if (track.IsPaused()) {
			continue;
		}
		track.Pause();
		events_.Push<event::AudioPaused>(audio_keys_.at(track.GetId()), track.GetInstanceId());
	}
}

void AudioSystem::ResumeAll() {
	for (auto& track : tracks_) {
		if (!track.IsPaused()) {
			continue;
		}
		track.Resume();
		events_.Push<event::AudioResumed>(audio_keys_.at(track.GetId()), track.GetInstanceId());
	}
}

void AudioSystem::StopAll() {
	for (const auto& track : tracks_) {
		events_.Push<event::AudioStopped>(audio_keys_.at(track.GetId()), track.GetInstanceId());
	}
	tracks_.clear();
	fade_targets_.clear();
}

bool AudioSystem::IsAnyPlaying() const {
	return std::ranges::any_of(tracks_, [](const auto& track) { return track.IsPlaying(); });
}

void AudioSystem::Update() {
	std::erase_if(tracks_, [&](auto& track) {
		bool finished{ track.IsFinished() };
		const auto& key{ audio_keys_.at(track.GetId()) };
		std::size_t loops{ track.TakeCompletedLoops() };
		for (std::size_t i{ 0 }; i < loops; ++i) {
			events_.Push<event::AudioLoopCompleted>(key, track.GetInstanceId());
		}
		if (finished) {
			events_.Push<event::AudioFinished>(key, track.GetInstanceId());
		}
		return finished;
	});
}

} // namespace ptgn