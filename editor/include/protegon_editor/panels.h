#pragma once

namespace ptgn {

class Application;

namespace editor {

void DrawDockspace(Application& app);
void DrawHierarchyWindow(Application& app);
void DrawScenesWindow(Application& app);
void DrawInspectorWindow(Application& app);
void DrawGameWindow(Application& app);
void DrawEngineSettingsWindow(Application& app);
void DrawAssetsWindow(Application& app);

} // namespace editor

} // namespace ptgn