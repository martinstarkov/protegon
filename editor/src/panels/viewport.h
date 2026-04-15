#pragma once

#include "core/editor_context.h"

namespace ptgn::editor {

class ViewportPanel {
public:
	void OnRender(EditorContext& ctx);
};

} // namespace ptgn::editor