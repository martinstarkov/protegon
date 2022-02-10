#include "Sound.h"

#include "sound/SoundManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace sound {

void Load(const char* key, const char* path) {
	auto& sound_manager{ services::GetSoundManager() };
    sound_manager.LoadSound(math::Hash(key), path);
}

void Unload(const char* key) {
	auto& sound_manager{ services::GetSoundManager() };
    sound_manager.UnloadSound(math::Hash(key));
}

bool Exists(const char* key) {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.HasSound(math::Hash(key));
}

void Play(const char* key, int channel, int loops) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.PlaySound(math::Hash(key), channel, loops);
}

void Pause(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.PauseSound(channel);
}

void Resume(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.ResumeSound(channel);
}

void Stop(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.StopSound(channel);
}

void FadeIn(const char* key, int channel, int loops, milliseconds fade_time) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.FadeInSound(math::Hash(key), channel, loops, fade_time);
}

void FadeOut(int channel, milliseconds fade_time) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.FadeOutSound(channel, fade_time);
}

bool IsPlaying(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsSoundPlaying(channel);
}

bool IsPaused(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsSoundPaused(channel);
}

bool IsFading(int channel) {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsSoundFading(channel);
}

} // namespace sound

namespace music {

void Load(const char* key, const char* path) {
	auto& sound_manager{ services::GetSoundManager() };
    sound_manager.LoadMusic(math::Hash(key), path);
}

void Unload(const char* key) {
	auto& sound_manager{ services::GetSoundManager() };
    sound_manager.UnloadMusic(math::Hash(key));
}

bool Exists(const char* key) {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.HasMusic(math::Hash(key));
}

void Play(const char* key, int loops) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.PlayMusic(math::Hash(key), loops);
}

void Resume() {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.ResumeMusic();
}

void Pause() {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.PauseMusic();
}

void Stop() {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.StopMusic();
}

void FadeIn(const char* key, int loops, milliseconds fade_time) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.FadeInMusic(math::Hash(key), loops, fade_time);
}

void FadeOut(milliseconds fade_time) {
	const auto& sound_manager{ services::GetSoundManager() };
    sound_manager.FadeOutMusic(fade_time);
}

bool IsPlaying() {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsMusicPlaying();
}

bool IsPaused() {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsMusicPaused();
}

bool IsFading() {
	const auto& sound_manager{ services::GetSoundManager() };
    return sound_manager.IsMusicFading();
}

} // namespace music

} // namespace ptgn