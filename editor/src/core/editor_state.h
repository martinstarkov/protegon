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

struct EditorState {
	bool is_playing{ false };
	bool is_paused{ false };

	bool is_dirty{ false };

	ViewportState viewport;
};

} // namespace editor

} // namespace ptgn