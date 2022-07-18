#include "Music.h"

#include <cassert> // assert

#include "manager/MusicManager.h"
#include "math/Hash.h"

namespace ptgn {

namespace music {

void Load(const char* music_key, const char* music_path) {
	auto& music_manager{ manager::Get<MusicManager>() };
	music_manager.Load(math::Hash(music_key), music_path);
}

void Play(const char* music_key, int loops) {
	assert(Exists(music_key) && "Cannot play nonexistent music");
	const auto& music_manager{ manager::Get<MusicManager>() };
	music_manager.Get(math::Hash(music_key))->Play(loops);
}

void FadeIn(const char* music_key, int loops, milliseconds time) {
	assert(Exists(music_key) && "Cannot fade in nonexistent music");
	const auto& music_manager{ manager::Get<MusicManager>() };
	music_manager.Get(math::Hash(music_key))->FadeIn(loops, time);
}

bool Exists(const char* music_key) {
	const auto& music_manager{ manager::Get<MusicManager>() };
	return music_manager.Has(math::Hash(music_key));
}

void Pause() {
	MusicManager::Pause();
}

void Resume() {
	MusicManager::Resume();
}

void Stop() {
	MusicManager::Stop();
}

void FadeOut(milliseconds time) {
	MusicManager::FadeOut(time);
}

bool IsPlaying() {
	return MusicManager::IsPlaying();
}

bool IsPaused() {
	return MusicManager::IsPaused();
}

bool IsFading() {
	return MusicManager::IsFading();
}

} // namespace music

} // namespace ptgn