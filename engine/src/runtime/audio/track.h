#pragma once

#include <cstdint>
#include <cstdlib>

struct MIX_Audio;
struct MIX_Track;
struct MIX_Mixer;

namespace ptgn {

class AudioSystem;

namespace impl {

class Track {
public:
	/// @param loops -1 for infinite loops.
	Track(std::size_t id, MIX_Mixer* mixer, MIX_Audio* audio, std::int64_t loops);

	~Track() noexcept;

	Track(const Track&)			   = delete;
	Track& operator=(const Track&) = delete;

	Track(Track&& other) noexcept;

	Track& operator=(Track&& other) noexcept;

	MIX_Track* Get() const noexcept;

	std::size_t GetId() const noexcept;

	void StopImmediate();

private:
	std::size_t id_{ 0 };
	MIX_Track* track_{ nullptr };
};

} // namespace impl

} // namespace ptgn