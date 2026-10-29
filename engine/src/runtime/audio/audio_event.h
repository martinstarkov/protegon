#pragma once

#include <cstddef>

#include "runtime/asset/asset_key.h"

namespace ptgn::event {

/// @brief Audio events are application events. instance_id distinguishes overlapping plays.
struct AudioStarted {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioFinished {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioStopped {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioPaused {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioResumed {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioLoopCompleted {
	AudioKey audio{};
	std::size_t instance_id{ 0 };
};

struct AudioFadeCompleted {
	AudioKey audio{};
};

struct AudioVolumeChanged {
	AudioKey audio{};
	float volume{ 1.0f };
};

} // namespace ptgn::event
