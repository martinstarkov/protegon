#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "core/util/file.h"

namespace ptgn {

class Window;

class FileDialog {
public:
	struct Filter {
		std::string name;
		std::string spec; // e.g. "png,jpg,jpeg" or "scene,ptgn"
	};

	struct Options {
		// Used by open/save dialogs. Ignored by folder-pick dialogs.
		std::vector<Filter> filters;

		// Initial directory shown by the dialog.
		// Used by open/save/folder dialogs.
		std::optional<path> default_path;

		// Suggested file name for save dialogs.
		// Ignored by open/folder dialogs.
		std::optional<std::string> default_name;
	};

	using Error = std::string;

	template <typename T>
	using Result = std::expected<std::optional<T>, Error>;

	[[nodiscard]] Result<path> OpenFile(const Options& options = {}) const;
	[[nodiscard]] Result<std::vector<path>> OpenFiles(const Options& options = {}) const;
	[[nodiscard]] Result<path> SaveFile(const Options& options = {}) const;
	[[nodiscard]] Result<path> OpenFolder(const Options& options = {}) const;
	[[nodiscard]] Result<std::vector<path>> OpenFolders(const Options& options = {}) const;

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