#include "Sound.h"

#include <cassert> // assert

#include "manager/SoundManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace sound {

void Load(const char* sound_key, const char* sound_path) {
	auto& sound_manager{ manager::Get<SoundManager>() };
	sound_manager.Load(math::Hash(sound_key), sound_path);
}

void Play(const char* sound_key, int channel, int loops) {
	assert(Exists(sound_key) && "Cannot play nonexistent sound");
	const auto& sound_manager{ manager::Get<SoundManager>() };
	sound_manager.Get(math::Hash(sound_key))->Play(channel, loops);
}

void FadeIn(const char* sound_key, int channel, int loops, milliseconds time) {
	assert(Exists(sound_key) && "Cannot fade in nonexistent sound");
	const auto& sound_manager{ manager::Get<SoundManager>() };
	sound_manager.Get(math::Hash(sound_key))->FadeIn(channel, loops, time);
}

bool Exists(const char* sound_key) {
	const auto& sound_manager{ manager::Get<SoundManager>() };
	return sound_manager.Has(math::Hash(sound_key));
}

void Pause(int channel) {
	SoundManager::Pause(channel);
}

void Resume(int channel) {
	SoundManager::Resume(channel);
}

void Stop(int channel) {
	SoundManager::Stop(channel);
}

void FadeOut(int channel, milliseconds time) {
	SoundManager::FadeOut(channel, time);
}

bool IsPlaying(int channel) {
	return SoundManager::IsPlaying(channel);
}

bool IsPaused(int channel) {
	return SoundManager::IsPaused(channel);
}

bool IsFading(int channel) {
	return SoundManager::IsFading(channel);
}

} // namespace sound

} // namespace ptgn