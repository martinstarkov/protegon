#include "Music.h"

#include "debugging/Debug.h"

#include <SDL_mixer.h>

namespace ptgn {

namespace internal {

Music::Music(const char* music_path) {
	assert(music_path != "" && "Cannot load empty music path into the music manager");
	assert(debug::FileExists(music_path) && "Cannot load music with non-existent file path into the music manager");
	music_ = Mix_LoadMUS(music_path);
	if (music_ == NULL) {
		debug::PrintLine("Failed to load music into sound manager: ", Mix_GetError());
	}
}

Music::~Music() {
	Mix_FreeMusic(music_);
	music_ = nullptr;
}

void Music::Play(int loops) const {
	Mix_PlayMusic(music_, loops);
}

void Music::Stop() const {
	Mix_HaltMusic();
}

void Music::FadeIn(int loops, milliseconds time) const {
	Mix_FadeInMusic(music_, loops, time.count());
}

void Music::FadeOut(milliseconds time) const {
	Mix_FadeOutMusic(time.count());
}

void Music::Pause() const {
	Mix_PauseMusic();
}

void Music::Resume() const {
	Mix_ResumeMusic();
}

bool Music::IsPlaying() const {
	return Mix_PlayingMusic();
}

bool Music::IsPaused() const {
	return Mix_PausedMusic();
}

bool Music::IsFading() const {
	switch (Mix_FadingMusic()) {
		case MIX_NO_FADING:  return false;
		case MIX_FADING_OUT: return true;
		case MIX_FADING_IN:  return true;
		default:             return false;
	}
}

Music::operator Mix_Music*() const {
	return music_;
}

} // namespace internal

} // namespace ptgn