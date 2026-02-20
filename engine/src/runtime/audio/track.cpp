#include "runtime/audio/track.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <cstdint>

#include "core/assert.h"
#include "runtime/audio/audio_system.h"

namespace ptgn::impl {

Track::Track(MIX_Mixer* mixer, MIX_Audio* audio, std::int64_t loops) {
	PTGN_ASSERT(loops == -1 || loops >= 0, "Loops cannot be negative unless -1 (infinite)");

	track_ = MIX_CreateTrack(mixer);
	PTGN_ASSERT(track_, SDL_GetError());

	PTGN_ASSERT(MIX_SetTrackAudio(track_, audio), SDL_GetError());

	SDL_PropertiesID props = SDL_CreateProperties();
	PTGN_ASSERT(props != 0);

	PTGN_ASSERT(SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops));

	PTGN_ASSERT(MIX_PlayTrack(track_, props), SDL_GetError());

	SDL_DestroyProperties(props);
}

Track::~Track() noexcept {
	StopImmediate();
}

Track::Track(Track&& other) noexcept : track_{ other.track_ } {
	other.track_ = nullptr;
}

Track& Track::operator=(Track&& other) noexcept {
	if (this != &other) {
		StopImmediate();
		track_		 = other.track_;
		other.track_ = nullptr;
	}
	return *this;
}

MIX_Track* Track::Get() const noexcept {
	return track_;
}

void Track::StopImmediate() {
	if (track_) {
		MIX_DestroyTrack(track_);
		track_ = nullptr;
	}
}

void Track::SetStoppedCallback(
	void (*cb)(void* userdata, MIX_Track* track), AudioSystem* self, std::size_t id
) {
	cbdata_.self = self;
	cbdata_.id	 = id;

	auto success{ MIX_SetTrackStoppedCallback(track_, cb, &cbdata_) };
	PTGN_ASSERT(success, SDL_GetError());
}

} // namespace ptgn::impl