#include "platform/file_dialog.h"

#include <concepts>
#include <filesystem>

#include "app/application.h"
#include "app/editor.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/interaction/interaction_system.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button.h"

using namespace ptgn;

namespace {

template <typename T>
void LogFileDialogResult(const char* label, const FileDialog::Result<T>& result) {
	if (!result) {
		PTGN_ERROR(label, " failed: ", result.error());
		return;
	}

	if (!*result) {
		PTGN_LOG(label, " cancelled");
		return;
	}

	if constexpr (std::same_as<T, path>) {
		PTGN_LOG(label, " selected: ", (**result).string());
	} else {
		PTGN_LOG(label, " selected ", (**result).size(), " paths:");
		for (const auto& path : **result) {
			PTGN_LOG("  - ", path.string());
		}
	}
}

} // namespace

class FileDialogDemoScene : public Scene {
public:
	void OnEnter() override {
		ctx().debug.settings.interaction.draw_enabled = true;

		const Origin button_origin{ Origin::Center };
		const V2_int button_size{ 360, 72 };

		CreateButton(*this, V2_float{ 0, -220 }, button_size, button_origin)
			.OnPress([](auto e) {
				const auto result =
						e.button.GetScene().ctx().window.file.OpenFile({
							.filters =
								{
									{ "Images", "png,jpg,jpeg,bmp,tga" },
									{ "Scenes", "scene,ptgn,json" },
									{ "All Files", "*" },
								},
							.default_path = "assets",
						});

				LogFileDialogResult("OpenFile", result);
			})
			.Text("Open File")
			.BackgroundShape(button_size)
			.BackgroundColor(color::LightBlue, color::Blue, color::DarkBlue);

		CreateButton(*this, V2_float{ 0, -110 }, button_size, button_origin)
			.OnPress([](auto e) {
				const auto result =
						e.button.GetScene().ctx().window.file.OpenFiles({
							.filters =
								{
									{ "Audio", "wav,ogg,mp3,flac" },
									{ "Images", "png,jpg,jpeg,bmp,tga" },
								},
							.default_path = "assets",
						});

				LogFileDialogResult("OpenFiles", result);
			})
			.Text("Open Files")
			.BackgroundShape(button_size)
			.BackgroundColor(color::LightRed, color::Red, color::DarkRed);

		CreateButton(*this, V2_float{ 0, 0 }, button_size, button_origin)
			.OnPress([](auto e) {
				const auto result =
						e.button.GetScene().ctx().window.file.SaveFile({
							.filters =
								{
									{ "Scene Files", "scene,ptgn,json" },
									{ "Text Files", "txt" },
								},
							.default_path = "assets",
							.default_name = "untitled.scene",
						});

				LogFileDialogResult("SaveFile", result);
			})
			.Text("Save File")
			.BackgroundShape(button_size)
			.BackgroundColor(color::LightGreen, color::Green, color::DarkGreen);

		CreateButton(*this, V2_float{ 0, 110 }, button_size, button_origin)
			.OnPress([](auto e) {
				const auto result = e.button.GetScene().ctx().window.file.OpenFolder(
					{
						.default_path = "assets",
					}
				);

				LogFileDialogResult("OpenFolder", result);
			})
			.Text("Open Folder")
			.BackgroundShape(button_size)
			.BackgroundColor(color::Pink, color::Red, color::DarkRed);

		CreateButton(*this, V2_float{ 0, 220 }, button_size, button_origin)
			.OnPress([](auto e) {
				const auto result = e.button.GetScene().ctx().window.file.OpenFolders(
					{
						.default_path = "default_path",
					}
				);

				LogFileDialogResult("OpenFolders", result);
			})
			.Text("Open Folders")
			.BackgroundShape(button_size)
			.BackgroundColor(color::LightPurple, color::Purple, color::DarkPurple);
	}
};

int main(int, char**) {
	Application app{ "FileDialogDemoScene" };
	PTGN_WITH_EDITOR(app, false);
	app.StartWith<FileDialogDemoScene>();
}