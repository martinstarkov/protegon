#pragma once

#include <optional>
#include <string>

#include "editor/editor_selection.h"

namespace ptgn::editor {

class EditorContext;

class InspectorPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	std::optional<EditorSelection> previous_selection_;
	std::optional<ScreenEffectSelection> previous_screen_effect_selection_;
	std::string previous_tile_entry_id_;
};

} // namespace ptgn::editor
