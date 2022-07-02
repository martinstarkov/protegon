#include "Sound.h"

#include "managers/SoundManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace sound {

void Load(const char* sound_key, const char* sound_path) {
	auto& sound_manager{ internal::managers::GetManager<internal::managers::SoundManager>() };
	sound_manager.Load(math::Hash(sound_key), new internal::Sound{ sound_path });
}

void Unload(const char* sound_key) {
	auto& sound_manager{ internal::managers::GetManager<internal::managers::SoundManager>() };
    sound_manager.Unload(math::Hash(sound_key));
}

bool Exists(const char* sound_key) {
	const auto& sound_manager{ internal::managers::GetManager<internal::managers::SoundManager>() };
    return sound_manager.Has(math::Hash(sound_key));
}

void Play(const char* sound_key, int channel, int loops) {
	const auto& sound_manager{ internal::managers::GetManager<internal::managers::SoundManager>() };
	const auto key{ math::Hash(sound_key) };
	assert(sound_manager.Has(key));
	const internal::Sound* sound = sound_manager.Get(key);
    sound->Play(channel, loops);
}

void Pause(int channel) {
	internal::managers::SoundManager::Pause(channel);
}

void Resume(int channel) {
	internal::managers::SoundManager::Resume(channel);
}

void Stop(int channel) {
	internal::managers::SoundManager::Stop(channel);
}

void FadeIn(const char* sound_key, int channel, int loops, milliseconds fade_time) {
	const auto& sound_manager{ internal::managers::GetManager<internal::managers::SoundManager>() };
	const auto key{ math::Hash(sound_key) };
	assert(sound_manager.Has(key));
	const internal::Sound* sound = sound_manager.Get(key);
	sound->FadeIn(channel, loops, fade_time);
}

void FadeOut(int channel, milliseconds fade_time) {
	internal::managers::SoundManager::FadeOut(channel, fade_time);
}

bool IsPlaying(int channel) {
	return internal::managers::SoundManager::IsPlaying(channel);
}

bool IsPaused(int channel) {
	return internal::managers::SoundManager::IsPaused(channel);
}

bool IsFading(int channel) {
	return internal::managers::SoundManager::IsFading(channel);
}

} // namespace sound

namespace music {

void Load(const char* music_key, const char* music_path) {
	auto& music_manager{ internal::managers::GetManager<internal::managers::MusicManager>() };
	music_manager.Load(math::Hash(music_key), new internal::Music{ music_path });
}

void Unload(const char* music_key) {
	auto& music_manager{ internal::managers::GetManager<internal::managers::MusicManager>() };
	music_manager.Unload(math::Hash(music_key));
}

bool Exists(const char* music_key) {
	const auto& music_manager{ internal::managers::GetManager<internal::managers::MusicManager>() };
	return music_manager.Has(math::Hash(music_key));
}

void Play(const char* music_key, int loops) {
	const auto& music_manager{ internal::managers::GetManager<internal::managers::MusicManager>() };
	const auto key{ math::Hash(music_key) };
	assert(music_manager.Has(key));
	const internal::Music* music = music_manager.Get(key);
	music->Play(loops);
}
void Resume() {
	internal::managers::MusicManager::Resume();
}

void Pause() {
	internal::managers::MusicManager::Pause();
}

void Stop() {
	internal::managers::MusicManager::Stop();
}

void FadeIn(const char* music_key, int loops, milliseconds fade_time) {
	const auto& music_manager{ internal::managers::GetManager<internal::managers::MusicManager>() };
	const auto key{ math::Hash(music_key) };
	assert(music_manager.Has(key));
	const internal::Music* music = music_manager.Get(key);
	music->FadeIn(loops, fade_time);
}

void FadeOut(milliseconds fade_time) {
	internal::managers::MusicManager::FadeOut(fade_time);
}


bool IsPlaying(int channel) {
	return internal::managers::MusicManager::IsPlaying();
}

bool IsPaused(int channel) {
	return internal::managers::MusicManager::IsPaused();
}

bool IsFading(int channel) {
	return internal::managers::MusicManager::IsFading();
}

} // namespace music

} // namespace ptgn