#include "protegon/audio.h"

#include "protegon/game.h"
#include "SDL_mixer.h"
#include "utility/debug.h"

namespace ptgn {

namespace impl {

struct MixMusicDeleter {
	void operator()(Mix_Music* music) {
		if (game.sdl_instance_.SDLMixerIsInitialized()) {
			Mix_FreeMusic(music);
		}
	}
};

struct MixChunkDeleter {
	void operator()(Mix_Chunk* sound) {
		if (game.sdl_instance_.SDLMixerIsInitialized()) {
			Mix_FreeChunk(sound);
		}
	}
};

} // namespace impl

Music::Music(const path& music_path) {
	PTGN_ASSERT(
		FileExists(music_path),
		"Cannot create music from a nonexistent music path: ", music_path.string()
	);
	instance_ = { Mix_LoadMUS(music_path.string().c_str()), impl::MixMusicDeleter{} };
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Music::Play(int loops) const {
	PTGN_ASSERT(IsValid(), "Cannot play uninitialized or destroyed music");
	Mix_PlayMusic(instance_.get(), loops);
}

void Music::FadeIn(int loops, milliseconds time) const {
	PTGN_ASSERT(IsValid(), "Cannot fade in uninitialized or destroyed music");
	const auto time_int = std::chrono::duration_cast<duration<int, milliseconds::period>>(time);
	Mix_FadeInMusic(instance_.get(), loops, time_int.count());
}

Sound::Sound(const path& sound_path) {
	PTGN_ASSERT(
		FileExists(sound_path),
		"Cannot create sound from a nonexistent sound path: ", sound_path.string()
	);
	instance_ = { Mix_LoadWAV(sound_path.string().c_str()), impl::MixChunkDeleter{} };
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Sound::Play(int channel, int loops) const {
	PTGN_ASSERT(IsValid(), "Cannot play uninitialized or destroyed sound");
	Mix_PlayChannel(channel, instance_.get(), loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) const {
	PTGN_ASSERT(IsValid(), "Cannot fade in uninitialized or destroyed sound");
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeInChannel(channel, instance_.get(), loops, time_int.count());
}

} // namespace ptgn