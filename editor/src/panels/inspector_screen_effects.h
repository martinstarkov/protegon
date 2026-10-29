#pragma once

#include "editor/editor_selection.h"

namespace ptgn::editor {

class EditorContext;

namespace inspector {

void DrawScreenEffectInspector(EditorContext& ctx, const ScreenEffectSelection& selection);

} // namespace inspector

} // namespace ptgn::editor
