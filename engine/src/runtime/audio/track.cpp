#include "runtime/audio/track.h"

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3_mixer/SDL_mixer.h>

#include <cstdint>
#include <optional>
#include <utility>

#include "core/assert.h"

namespace ptgn::impl {

Track::Track(
	std::size_t id, MIX_Mixer* mixer, MIX_Audio* audio, std::optional<std::int64_t> loops
) :
	id_{ id } {
	PTGN_ASSERT(
		!loops.has_value() || *loops >= 0, "Audio loop count must be positive or infinite (nullopt)"
	);

	track_ = MIX_CreateTrack(mixer);
	PTGN_ASSERT(track_, SDL_GetError());

	auto set_audio{ MIX_SetTrackAudio(track_, audio) };
	PTGN_ASSERT(set_audio, SDL_GetError());

	SDL_PropertiesID props = SDL_CreateProperties();
	PTGN_ASSERT(props != 0);

	auto loop_set{ SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, loops.value_or(-1)) };
	PTGN_ASSERT(loop_set, SDL_GetError());

	auto play_track{ MIX_PlayTrack(track_, props) };
	PTGN_ASSERT(play_track, SDL_GetError());

	SDL_DestroyProperties(props);
}

Track::~Track() noexcept {
	StopImmediate();
}

Track::Track(Track&& other) noexcept :
	id_{ std::exchange(other.id_, 0) }, track_{ std::exchange(other.track_, nullptr) } {}

Track& Track::operator=(Track&& other) noexcept {
	if (this != &other) {
		StopImmediate();
		id_	   = std::exchange(other.id_, 0);
		track_ = std::exchange(other.track_, nullptr);
	}
	return *this;
}

MIX_Track* Track::Get() const noexcept {
	return track_;
}

std::size_t Track::GetId() const noexcept {
	return id_;
}

void Track::StopImmediate() {
	if (track_) {
		MIX_DestroyTrack(track_);
		track_ = nullptr;
	}
}

} // namespace ptgn::impl