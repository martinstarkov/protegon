#pragma once

namespace ptgn {

class Application;
class EditorLayer;

namespace editor {

void DrawDockspace(EditorLayer& layer, Application& app);
void DrawHierarchyWindow(EditorLayer& layer, Application& app);
void DrawScenesWindow(EditorLayer& layer, Application& app);
void DrawInspectorWindow(EditorLayer& layer, Application& app);
void DrawGameWindow(EditorLayer& layer, Application& app);
void DrawEngineSettingsWindow(EditorLayer& layer, Application& app);
void DrawAssetsWindow(EditorLayer& layer, Application& app);

} // namespace editor

} // namespace ptgn