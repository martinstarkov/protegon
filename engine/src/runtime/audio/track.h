#pragma once

#include <cstdint>
#include <memory>
#include <optional>

#include "core/util/file.h"

struct MIX_Audio;
struct ma_engine;
struct ma_sound;

namespace ptgn {

class AudioSystem;

namespace impl {

class Track {
public:
	/// @param loops Nullopt for infinite loops.
	Track(
		std::size_t id, ma_engine* engine, const path& audio_path, std::optional<std::int64_t> loops
	);

	~Track() noexcept;

	Track(const Track&)			   = delete;
	Track& operator=(const Track&) = delete;

	Track(Track&& other) noexcept;
	Track& operator=(Track&& other) noexcept;

	[[nodiscard]] ma_sound* Get() const noexcept;
	[[nodiscard]] std::size_t GetId() const noexcept;

	void Pause();
	void Resume();
	void StopImmediate();

	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsFinished() const;

	void SetVolume(float volume);
	[[nodiscard]] float GetVolume() const;

	void SetPitch(float pitch);

private:
	void RestartFromBeginning();

	std::size_t id_{ 0 };
	std::unique_ptr<ma_sound> sound_;
	bool paused_{ false };

	/// @brief Finite-loop emulation:
	/// nullopt, infinite looping handled by miniaudio directly
	/// N, play once + restart N additional times
	mutable std::optional<std::int64_t> remaining_loops_;

	struct OggDecoder;

	std::unique_ptr<OggDecoder> vorbis_;
};

} // namespace impl

} // namespace ptgn