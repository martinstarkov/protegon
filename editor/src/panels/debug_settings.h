#pragma once

namespace ptgn::editor {

class EditorContext;

class DebugSettingsPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	bool show_imgui_metrics_{ false };
};

} // namespace ptgn::editor