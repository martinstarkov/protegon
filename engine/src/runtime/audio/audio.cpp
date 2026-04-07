#include "runtime/audio/audio.h"

#include <SDL3_mixer/SDL_mixer.h>

namespace ptgn {

namespace impl {

void MIX_AudioDeleter::operator()(MIX_Audio* audio) const {
	MIX_DestroyAudio(audio);
}

} // namespace impl

} // namespace ptgn