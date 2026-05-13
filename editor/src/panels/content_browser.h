#pragma once

#include "core/editor_context.h"

namespace ptgn::editor {

class ContentBrowserPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	void DrawContentBrowser(EditorContext& ctx);
};

} // namespace ptgn::editor