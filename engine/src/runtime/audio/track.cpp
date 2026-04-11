#include "runtime/audio/track.h"

#include <miniaudio.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/assert.h"
#include "core/util/file.h"

namespace ptgn::impl {

Track::Track(
	std::size_t id, ma_engine* engine, const path& audio_path, std::optional<std::int64_t> loops
) :
	id_{ id }, sound_{ std::make_unique<ma_sound>() }, remaining_loops_{ loops } {
	PTGN_ASSERT(engine, "Audio engine must be valid");
	PTGN_ASSERT(
		!loops.has_value() || *loops >= 0, "Audio loop count must be positive or infinite (nullopt)"
	);

	// Disable spatialization since your old API is plain 2D playback.
	ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;

	PTGN_ASSERT(
		FileExists(audio_path), "Cannot create audio from invalid path: ", audio_path.string()
	);

	auto result = ma_sound_init_from_file(
		engine, audio_path.string().c_str(), flags, nullptr, nullptr, sound_.get()
	);

	PTGN_ASSERT(result == MA_SUCCESS, "ma_sound_init_from_file() failed");

	if (!loops.has_value()) {
		// Infinite loop.
		ma_sound_set_looping(sound_.get(), MA_TRUE);
	}

	result = ma_sound_start(sound_.get());
	PTGN_ASSERT(result == MA_SUCCESS, "ma_sound_start() failed");
}

Track::~Track() noexcept {
	StopImmediate();
}

Track::Track(Track&& other) noexcept :
	id_{ std::exchange(other.id_, 0) },
	sound_{ std::move(other.sound_) },
	paused_{ std::exchange(other.paused_, false) },
	remaining_loops_{ std::exchange(other.remaining_loops_, std::nullopt) } {}

Track& Track::operator=(Track&& other) noexcept {
	if (this != &other) {
		StopImmediate();
		id_				 = std::exchange(other.id_, 0);
		sound_			 = std::move(other.sound_);
		paused_			 = std::exchange(other.paused_, false);
		remaining_loops_ = std::exchange(other.remaining_loops_, std::nullopt);
	}
	return *this;
}

ma_sound* Track::Get() const noexcept {
	return sound_.get();
}

std::size_t Track::GetId() const noexcept {
	return id_;
}

void Track::Pause() {
	if (!sound_ || paused_) {
		return;
	}

	// miniaudio has stop/start, and stop does not rewind.
	auto result = ma_sound_stop(sound_.get());
	PTGN_ASSERT(result == MA_SUCCESS, "ma_sound_stop() failed while pausing");

	paused_ = true;
}

void Track::Resume() {
	if (!sound_ || !paused_) {
		return;
	}

	auto result = ma_sound_start(sound_.get());
	PTGN_ASSERT(result == MA_SUCCESS, "ma_sound_start() failed while resuming");

	paused_ = false;
}

void Track::StopImmediate() {
	if (sound_) {
		ma_sound_uninit(sound_.get());
		sound_.reset();
		paused_ = false;
	}
}

bool Track::IsPlaying() const {
	return sound_ && ma_sound_is_playing(sound_.get()) == MA_TRUE;
}

bool Track::IsPaused() const {
	return sound_ && paused_;
}

void Track::SetVolume(float volume) {
	if (sound_) {
		ma_sound_set_volume(sound_.get(), volume);
	}
}

float Track::GetVolume() const {
	return sound_ ? ma_sound_get_volume(sound_.get()) : 0.0f;
}

void Track::SetPitch(float pitch) {
	if (sound_) {
		ma_sound_set_pitch(sound_.get(), pitch);
	}
}

void Track::RestartFromBeginning() {
	PTGN_ASSERT(sound_, "Cannot restart an invalid sound");

	auto seek_result = ma_sound_seek_to_pcm_frame(sound_.get(), 0);
	PTGN_ASSERT(seek_result == MA_SUCCESS, "ma_sound_seek_to_pcm_frame() failed");

	auto start_result = ma_sound_start(sound_.get());
	PTGN_ASSERT(start_result == MA_SUCCESS, "ma_sound_start() failed");
}

bool Track::IsFinished() const {
	if (!sound_) {
		return true;
	}

	if (paused_) {
		return false;
	}

	// Infinite loops never finish unless stopped explicitly.
	if (!remaining_loops_.has_value()) {
		return false;
	}

	if (ma_sound_is_playing(sound_.get()) == MA_TRUE) {
		return false;
	}

	// Not playing. If we're at the natural end, we may need to emulate finite loops.
	if (ma_sound_at_end(sound_.get()) == MA_TRUE) {
		if (*remaining_loops_ > 0) {
			--(*remaining_loops_);
			// NOSONAR
			const_cast<Track*>(this)->RestartFromBeginning();
			return false;
		}
		return true;
	}

	return false;
}

} // namespace ptgn::impl