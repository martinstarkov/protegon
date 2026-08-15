#pragma once

#include <optional>

#include "editor/editor_selection.h"

namespace ptgn::editor {

class EditorContext;

class InspectorPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	std::optional<ScreenEffectSelection> previous_screen_effect_selection_;
};

} // namespace ptgn::editor
