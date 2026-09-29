#pragma once

#include <imgui.h>

#include <cstdint>

namespace ptgn::editor {

enum class EditorIcon : std::uint8_t {
	Play,
	Stop,
	Pause,
	Step,
	Camera,
	Restart,
	Reset,
};

void DrawEditorIcon(ImDrawList* draw, EditorIcon icon, ImVec2 min, float extent, ImU32 color);

bool DrawEditorIconButton(const char* id, EditorIcon icon, const char* tooltip = nullptr);

} // namespace ptgn::editor
