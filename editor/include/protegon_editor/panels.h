#pragma once

namespace ptgn {

class EditorLayer;

namespace editor {

void DrawDockspace(EditorLayer& layer);
void DrawHierarchyWindow(EditorLayer& layer);
void DrawScenesWindow(EditorLayer& layer);
void DrawInspectorWindow(EditorLayer& layer);
void DrawGameWindow(EditorLayer& layer);
void DrawEngineSettingsWindow(EditorLayer& layer);
void DrawAssetsWindow(EditorLayer& layer);

} // namespace editor

} // namespace ptgn