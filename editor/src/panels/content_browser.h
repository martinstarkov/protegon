#pragma once

#include "core/editor_context.h"
#include "panels/render_graph_visualizer.h"

namespace ptgn::editor {

class ContentBrowserPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	void DrawContentBrowser(EditorContext& ctx);

	RenderGraphVisualizer render_graph_visualizer_;
};

} // namespace ptgn::editor