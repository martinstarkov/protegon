#pragma once

#include <array>
#include <cstdint>

namespace ptgn::editor {

class EditorContext;
class UndoStack;

enum class SettingsPage : std::uint8_t {
	ProjectDisplay,
	ProjectRendering,
	EditorGeneral,
	DebugInteraction,
	DebugCollision,
	DebugText,
	DebugVisibility,
};

class SettingsWindow {
public:
	void Open(SettingsPage page);
	void OnRender(EditorContext& ctx);

private:
	std::array<char, 256> search_{};
	SettingsPage selected_page_{ SettingsPage::ProjectDisplay };
	bool open_{ false };
	bool focus_requested_{ false };
};

class UndoHistoryWindow {
public:
	void Open();
	void OnRender(EditorContext& ctx, UndoStack& undo_stack);

private:
	bool open_{ false };
	bool focus_requested_{ false };
};

} // namespace ptgn::editor
