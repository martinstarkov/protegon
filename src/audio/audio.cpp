#include "protegon/audio.h"

#include <chrono>
#include <filesystem>
#include <ratio>
#include <string>

#include "core/sdl_instance.h"
#include "protegon/file.h"
#include "protegon/game.h"
#include "SDL_mixer.h"
#include "utility/debug.h"
#include "utility/handle.h"
#include "utility/time.h"

namespace ptgn {

namespace impl {

struct MixMusicDeleter {
	void operator()(Mix_Music* music) const {
		if (game.sdl_instance_.SDLMixerIsInitialized()) {
			Mix_FreeMusic(music);
		}
	}
};

struct MixChunkDeleter {
	void operator()(Mix_Chunk* sound) const {
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
	Create({ Mix_LoadMUS(music_path.string().c_str()), impl::MixMusicDeleter{} });
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Music::Play(int loops) {
	Mix_PlayMusic(&Get(), loops);
}

void Music::FadeIn(int loops, milliseconds time) {
	const auto time_int = std::chrono::duration_cast<duration<int, milliseconds::period>>(time);
	Mix_FadeInMusic(&Get(), loops, time_int.count());
}

Sound::Sound(const path& sound_path) {
	PTGN_ASSERT(
		FileExists(sound_path),
		"Cannot create sound from a nonexistent sound path: ", sound_path.string()
	);
	Create({ Mix_LoadWAV(sound_path.string().c_str()), impl::MixChunkDeleter{} });
	PTGN_ASSERT(IsValid(), Mix_GetError());
}

void Sound::Play(int channel, int loops) {
	Mix_PlayChannel(channel, &Get(), loops);
}

void Sound::FadeIn(int channel, int loops, milliseconds time) {
	const auto time_int = std::chrono::duration_cast<duration<int, std::milli>>(time);
	Mix_FadeInChannel(channel, &Get(), loops, time_int.count());
}

} // namespace ptgn