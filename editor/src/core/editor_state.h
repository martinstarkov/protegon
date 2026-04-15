#pragma once

#include "core/math/vector2.h"
#include "core/util/file.h"

namespace ptgn {

class Scene;

namespace editor {

struct ViewportState {
	V2_int size{ 0, 0 };
	bool focused{ false };
	bool hovered{ false };
};

struct EditorState {
	bool is_playing{ false };
	bool is_paused{ false };

	Scene* active_scene{ nullptr };
	path active_scene_path;
	bool is_dirty{ false };

	ViewportState viewport;
};

} // namespace editor

} // namespace ptgn