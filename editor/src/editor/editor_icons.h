#pragma once

#include <imgui.h>

#include <cstdint>

namespace ptgn::editor {

enum class EditorIcon : std::uint8_t {
	Play,
	Stop,
	Pause,
	Reset,
	StepForward,
	StepBackward,
	Camera,
	Entity,
	Tile,
	Visible,
	Hidden,
	Locked,
	Unlocked,
	Select,
	Move,
	Pencil,
	Brush,
	Line,
	Rectangle,
	Fill,
	Erase,
	Eyedropper,
	Target,
	Grid
};

struct EditorIconButtonOptions {
	const char* tooltip{ nullptr };
	bool selected{ false };
	bool compact{ false };
	bool interactive{ true };
	bool muted{ false };
	bool no_navigation{ false };
	float icon_extent{ 0.0f };
};

void DrawEditorIcon(ImDrawList* draw, EditorIcon icon, ImVec2 min, float extent, ImU32 color);

bool DrawEditorIconButton(const char* id, EditorIcon icon, const char* tooltip = nullptr);

bool DrawEditorIconButton(const char* id, EditorIcon icon, EditorIconButtonOptions options);

} // namespace ptgn::editor
