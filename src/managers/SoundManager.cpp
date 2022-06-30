#include "SoundManager.h"

#include "managers/SDLManager.h"

namespace ptgn {

namespace internal {

namespace managers {

SoundManager::SoundManager() {
	GetSDLManager();
}

MusicManager::MusicManager() {
	GetSDLManager();
}

} // namespace managers

managers::SoundManager& GetSoundManager() {
	static managers::SoundManager sound_manager;
	return sound_manager;
}

managers::MusicManager& GetMusicManager() {
	static managers::MusicManager music_manager;
	return music_manager;
}

} // namespace internal

} // namespace ptgn