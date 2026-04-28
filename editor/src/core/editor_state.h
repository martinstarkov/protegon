#pragma once

#include "core/util/file.h"
#include "renderer/pipeline/viewport.h"

namespace ptgn {

class Scene;

namespace editor {

struct ViewportState {
	Viewport viewport;
	bool focused{ false };
	bool hovered{ false };
};

enum class PlayState {
	Stopped,
	Playing,
	Paused
};

struct EditorState {
	bool is_playing{ false };
	bool is_paused{ false };

	bool is_dirty{ false };

	ViewportState viewport;

	PlayState play_state{ PlayState::Stopped };
	float time_scale{ 1.0f };
};

} // namespace editor

} // namespace ptgn