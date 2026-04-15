#include "platform/file_dialog.h"

#include <concepts>
#include <filesystem>

#include "app/application.h"
#include "core/graphics/color.h"
#include "core/log.h"
#include "core/math/geometry/origin.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "platform/window.h"
#include "runtime/ecs/entity.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
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

	if constexpr (std::same_as<T, std::filesystem::path>) {
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
		ctx().interaction.SetDebugSettings({ .draw_enabled = true });

		const Origin button_origin{ Origin::Center };
		const V2_int button_size{ 360, 72 };

		CreateButton(*this, V2_float{ 0, -220 }, button_size, button_origin)
			.OnPress([](auto button) {
				const auto result =
						button.GetScene().ctx().window.file.OpenFile({
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
			//.SetText("Open File")
			.SetBackgroundShape(button_size)
			.SetBackgroundColor(color::LightBlue)
			.SetBackgroundColor(color::Blue, ButtonState::Hover)
			.SetBackgroundColor(color::DarkBlue, ButtonState::Press);

		CreateButton(*this, V2_float{ 0, -110 }, button_size, button_origin)
			.OnPress([](auto button) {
				const auto result =
						button.GetScene().ctx().window.file.OpenFiles({
							.filters =
								{
									{ "Audio", "wav,ogg,mp3,flac" },
									{ "Images", "png,jpg,jpeg,bmp,tga" },
								},
							.default_path = "assets",
						});

				LogFileDialogResult("OpenFiles", result);
			})
			//.SetText("Open Files")
			.SetBackgroundShape(button_size)
			.SetBackgroundColor(color::LightRed)
			.SetBackgroundColor(color::Red, ButtonState::Hover)
			.SetBackgroundColor(color::DarkRed, ButtonState::Press);

		CreateButton(*this, V2_float{ 0, 0 }, button_size, button_origin)
			.OnPress([](auto button) {
				const auto result =
						button.GetScene().ctx().window.file.SaveFile({
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
			//.SetText("Save File")
			.SetBackgroundShape(button_size)
			.SetBackgroundColor(color::LightGreen)
			.SetBackgroundColor(color::Green, ButtonState::Hover)
			.SetBackgroundColor(color::DarkGreen, ButtonState::Press);

		CreateButton(*this, V2_float{ 0, 110 }, button_size, button_origin)
			.OnPress([](auto button) {
				const auto result = button.GetScene().ctx().window.file.OpenFolder({
					.default_path = "assets",
				});

				LogFileDialogResult("OpenFolder", result);
			})
			//.SetText("Open Folder")
			.SetBackgroundShape(button_size)
			.SetBackgroundColor(color::Pink)
			.SetBackgroundColor(color::Red, ButtonState::Hover)
			.SetBackgroundColor(color::DarkRed, ButtonState::Press);

		CreateButton(*this, V2_float{ 0, 220 }, button_size, button_origin)
			.OnPress([](auto button) {
				const auto result = button.GetScene().ctx().window.file.OpenFolders({
					.default_path = "default_path",
				});

				LogFileDialogResult("OpenFolders", result);
			})
			//.SetText("Open Folders")
			.SetBackgroundShape(button_size)
			.SetBackgroundColor(color::LightPurple)
			.SetBackgroundColor(color::Purple, ButtonState::Hover)
			.SetBackgroundColor(color::DarkPurple, ButtonState::Press);
	}
};

int main(int, char**) {
	Application game{ "FileDialogDemoScene" };
	game.StartWith<FileDialogDemoScene>();
}