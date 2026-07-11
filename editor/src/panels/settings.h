#pragma once

namespace ptgn::editor {

class EditorContext;

class EngineSettingsPanel {
public:
	void OnRender(EditorContext& ctx);
};

class DebugSettingsPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	bool show_imgui_metrics_{ false };
};

class EditorSettingsPanel {
public:
	void OnRender(EditorContext& ctx);
};

} // namespace ptgn::editor