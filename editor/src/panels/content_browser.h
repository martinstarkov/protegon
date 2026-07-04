#pragma once

namespace ptgn::editor {

class EditorContext;

class ContentBrowserPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	void DrawContentBrowser(EditorContext& ctx);
};

} // namespace ptgn::editor