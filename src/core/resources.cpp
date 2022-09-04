#include "protegon/resources.h"

#include <SDL_mixer.h>

#include "core/game.h"

namespace ptgn {

ResourceManagers& GetManagers() {
	return global::GetGame().managers;
}

namespace font {

void Unload(std::size_t key) {
	GetManagers().font.Unload(key);
}

bool Has(std::size_t key) {
	return GetManagers().font.Has(key);
}

std::shared_ptr<Font> Get(std::size_t key) {
	return GetManagers().font.Get(key);
}

void Clear() {
	GetManagers().font.Clear();
}

} // namespace font

namespace music {

void Unload(std::size_t key) {
	GetManagers().music.Unload(key);
}

bool Has(std::size_t key) {
	return GetManagers().music.Has(key);
}

std::shared_ptr<Music> Get(std::size_t key) {
	return GetManagers().music.Get(key);
}

void Clear() {
	GetManagers().music.Clear();
}

void Stop() {
	Mix_HaltMusic();
}

void FadeOut(milliseconds time) {
	Mix_FadeOutMusic(time.count());
}

void Pause() {
	Mix_PauseMusic();
}

void Resume() {
	Mix_ResumeMusic();
}

bool IsPlaying() {
	return Mix_PlayingMusic();
}

bool IsPaused() {
	return Mix_PausedMusic();
}

bool IsFading() {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:  return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:  return true;
		default:             return false;
	}
}

} // namespace music

namespace sound {

void Unload(std::size_t key) {
	GetManagers().sound.Unload(key);
}

bool Has(std::size_t key) {
	return GetManagers().sound.Has(key);
}

std::shared_ptr<Sound> Get(std::size_t key) {
	return GetManagers().sound.Get(key);
}

void Clear() {
	GetManagers().sound.Clear();
}

} // namespace sound

namespace text {

void Unload(std::size_t key) {
	GetManagers().text.Unload(key);
}

bool Has(std::size_t key) {
	return GetManagers().text.Has(key);
}

std::shared_ptr<Text> Get(std::size_t key) {
	return GetManagers().text.Get(key);
}

void Clear() {
	GetManagers().text.Clear();
}

} // namespace text

namespace texture {

void Unload(std::size_t key) {
	GetManagers().texture.Unload(key);
}

bool Has(std::size_t key) {
	return GetManagers().texture.Has(key);
}

std::shared_ptr<Texture> Get(std::size_t key) {
	return GetManagers().texture.Get(key);
}

void Clear() {
	GetManagers().texture.Clear();
}

} // namespace texture

} // namespace ptgn