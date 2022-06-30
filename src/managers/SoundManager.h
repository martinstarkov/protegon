#pragma once

#include "managers/Manager.h"
#include "sound/Music.h"
#include "sound/Sound.h"

namespace ptgn {

namespace internal {

namespace managers {

class SoundManager : public Manager<Sound> {
public:
    SoundManager();
};

class MusicManager : public Manager<Music> {
public:
    MusicManager();
};

} // namespace managers

managers::SoundManager& GetSoundManager();
managers::MusicManager& GetMusicManager();

} // namespace internal

} // namespace ptgn