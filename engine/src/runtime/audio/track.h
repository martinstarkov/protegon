#pragma once

#include <cstddef>
#include <cstdint>

struct MIX_Audio;
struct MIX_Track;
struct MIX_Mixer;

namespace ptgn {

class AudioSystem;

namespace impl {

struct CallbackData {
	AudioSystem* self{ nullptr };
	std::size_t id{ 0 };
};

class Track {
public:
	/// @param loops -1 for infinite loops.
	Track(MIX_Mixer* mixer, MIX_Audio* audio, std::int64_t loops);

	~Track() noexcept;

	Track(const Track&)			   = delete;
	Track& operator=(const Track&) = delete;

	Track(Track&& other) noexcept;

	Track& operator=(Track&& other) noexcept;

	MIX_Track* Get() const noexcept;

	void StopImmediate();

	void SetStoppedCallback(
		void (*cb)(void* userdata, MIX_Track* track), AudioSystem* self, std::size_t id
	);

private:
	MIX_Track* track_{ nullptr };

	CallbackData cbdata_{};
};

} // namespace impl

} // namespace ptgn