#include "Sound.h"

#include "managers/SoundManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace sound {

void Load(const char* key, const char* path) {
	auto& sound_manager{ internal::GetSoundManager() };
	sound_manager.Load(math::Hash(key), new internal::Sound{ path });
}

void Unload(const char* key) {
	auto& sound_manager{ internal::GetSoundManager() };
    sound_manager.Unload(math::Hash(key));
}

bool Exists(const char* key) {
	const auto& sound_manager{ internal::GetSoundManager() };
    return sound_manager.Has(math::Hash(key));
}

void Play(const char* key, int channel, int loops) {
	const auto& sound_manager{ internal::GetSoundManager() };
	assert(sound_manager.Has(math::Hash(key)));
	const internal::Sound* sound = sound_manager.Get(math::Hash(key));
    sound->Play(channel, loops);
}

void Pause(int channel) {
	// TODO: FIGURE ALL THIS SOUND STUFF OUT LATER...
}

void Resume(int channel) {
	const auto& sound_manager{ internal::GetSoundManager() };
    sound_manager.ResumeSound(channel);
}

void Stop(int channel) {
	const auto& sound_manager{ internal::GetSoundManager() };
    sound_manager.StopSound(channel);
}

void FadeIn(const char* key, int channel, int loops, milliseconds fade_time) {
	const auto& sound_manager{ internal::GetSoundManager() };
    sound_manager.FadeInSound(math::Hash(key), channel, loops, fade_time);
}

void FadeOut(int channel, milliseconds fade_time) {
	const auto& sound_manager{ internal::GetSoundManager() };
    sound_manager.FadeOutSound(channel, fade_time);
}

bool IsPlaying(int channel) {
	const auto& sound_manager{ internal::GetSoundManager() };
    return sound_manager.IsSoundPlaying(channel);
}

bool IsPaused(int channel) {
	const auto& sound_manager{ internal::GetSoundManager() };
    return sound_manager.IsSoundPaused(channel);
}

bool IsFading(int channel) {
	const auto& sound_manager{ internal::GetSoundManager() };
    return sound_manager.IsSoundFading(channel);
}

} // namespace sound

namespace music {

void Load(const char* key, const char* path) {
	auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.LoadMusic(math::Hash(key), path);
}

void Unload(const char* key) {
	auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.UnloadMusic(math::Hash(key));
}

bool Exists(const char* key) {
	const auto& sound_manager{ internal::GetMusicManager() };
    return sound_manager.HasMusic(math::Hash(key));
}

void Play(const char* key, int loops) {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.PlayMusic(math::Hash(key), loops);
}

void Resume() {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.ResumeMusic();
}

void Pause() {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.PauseMusic();
}

void Stop() {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.StopMusic();
}

void FadeIn(const char* key, int loops, milliseconds fade_time) {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.FadeInMusic(math::Hash(key), loops, fade_time);
}

void FadeOut(milliseconds fade_time) {
	const auto& sound_manager{ internal::GetMusicManager() };
    sound_manager.FadeOutMusic(fade_time);
}

bool IsPlaying() {
	const auto& sound_manager{ internal::GetMusicManager() };
    return sound_manager.IsMusicPlaying();
}

bool IsPaused() {
	const auto& sound_manager{ internal::GetMusicManager() };
    return sound_manager.IsMusicPaused();
}

bool IsFading() {
	const auto& sound_manager{ internal::GetMusicManager() };
    return sound_manager.IsMusicFading();
}

} // namespace music

} // namespace ptgn