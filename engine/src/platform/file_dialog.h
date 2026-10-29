#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "core/util/file.h"

namespace ptgn {

class Window;

struct FileDialogFilter {
	std::string name{};
	std::string spec{}; // e.g. "png,jpg,jpeg" or "scene,ptgn"
};

struct FileDialogOptions {
	/// @brief Used by open/save dialogs. Ignored by folder-pick dialogs.
	std::vector<FileDialogFilter> filters{};

	/// @brief Initial directory shown by the dialog.
	/// Used by open/save/folder dialogs.
	std::optional<path> default_path{};

	/// @brief Suggested file name for save dialogs.
	/// Ignored by open/folder dialogs.
	std::optional<std::string> default_name{};
};

class FileDialog {
public:
	using Error = std::string;
	using Filter = FileDialogFilter;
	using Options = FileDialogOptions;

	template <typename T>
	using Result = std::expected<std::optional<T>, Error>;

	[[nodiscard]] Result<path> OpenFile(Options options = {}) const;
	[[nodiscard]] Result<std::vector<path>> OpenFiles(Options options = {}) const;
	[[nodiscard]] Result<path> SaveFile(Options options = {}) const;
	[[nodiscard]] Result<path> OpenFolder(Options options = {}) const;
	[[nodiscard]] Result<std::vector<path>> OpenFolders(Options options = {}) const;

private:
	friend class Window;

	explicit FileDialog(Window& window);
	~FileDialog();
	FileDialog(const FileDialog&)				 = delete;
	FileDialog& operator=(const FileDialog&)	 = delete;
	FileDialog(FileDialog&&) noexcept			 = delete;
	FileDialog& operator=(FileDialog&&) noexcept = delete;

	Window& window_;
};

} // namespace ptgn