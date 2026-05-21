#include "platform/file_dialog.h"

#include <expected>
#include <vector>

#include "core/util/file.h"
#include "platform/window.h"

#ifdef __EMSCRIPTEN__

namespace ptgn {

FileDialog::FileDialog(Window& window) : window_{ window } {}

FileDialog::~FileDialog() {}

FileDialog::Result<path> FileDialog::OpenFile(const Options&) const {
	return std::unexpected("OpenFile is not supported on Emscripten");
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFiles(const Options&) const {
	return std::unexpected("OpenFiles is not supported on Emscripten");
}

FileDialog::Result<path> FileDialog::SaveFile(const Options&) const {
	return std::unexpected("SaveFile is not supported on Emscripten");
}

FileDialog::Result<path> FileDialog::OpenFolder(const Options&) const {
	return std::unexpected("OpenFolder is not supported on Emscripten");
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFolders(const Options&) const {
	return std::unexpected("OpenFolders is not supported on Emscripten");
}

} // namespace ptgn

#else

#include <nfd.h>
#include <nfd_glfw3.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <nfd.hpp>
#include <optional>
#include <string>
#include <utility>

#include "core/assert.h"
#include "core/log.h"

namespace ptgn {

namespace {

template <typename From, typename Into, typename F, typename FTransform>
std::expected<std::optional<Into>, std::string> GetResult(
	GLFWwindow* glfw_window, const FileDialog::Options& options, F&& func,
	FTransform&& transform_func
) {
	PTGN_ASSERT(glfw_window, "Window must be initialized before opening a file dialog");

	From out{};

	std::vector<nfdfilteritem_t> filters;
	filters.reserve(options.filters.size());
	for (const auto& filter : options.filters) {
		filters.emplace_back(filter.name.c_str(), filter.spec.c_str());
	}

	nfdwindowhandle_t native_window;

	auto success{ NFD_GetNativeWindowFromGLFWWindow(glfw_window, &native_window) };
	PTGN_ASSERT(success, "Failed to get native window handle from GLFW window for NFD");

	std::u8string path_storage;
	const nfdu8char_t* path{ nullptr };

	if (options.default_path.has_value()) {
		path_storage = options.default_path->u8string();
		path		 = reinterpret_cast<const char*>(path_storage.c_str());
	}

	const nfdu8char_t* name{ options.default_name.has_value() ? options.default_name->c_str()
															  : nullptr };

	auto result{ func(
		out, filters.data(), static_cast<std::uint32_t>(filters.size()), path, name, native_window
	) };

	if (result == NFD_OKAY) {
		return transform_func(std::move(out));
	} else if (result == NFD_CANCEL) {
		return std::nullopt;
	} else {
		return std::unexpected(NFD::GetError());
	}
}

std::vector<path> ToPaths(NFD::UniquePathSet path_set) {
	std::vector<path> paths;
	nfdpathsetsize_t count{ 0 };
	NFD::PathSet::Count(path_set, count);
	paths.reserve(count);
	for (nfdpathsetsize_t i{ 0 }; i < count; ++i) {
		NFD::UniquePathSetPathU8 path;
		NFD::PathSet::GetPath(path_set, i, path);
		paths.emplace_back(path.get());
	}
	return paths;
}

} // namespace

FileDialog::FileDialog(Window& window) : window_{ window } {
	if (NFD::Init() != NFD_OKAY) {
		PTGN_ERROR("NFD Init failed: ", NFD::GetError());
	}
	if (!NFD_SetDisplayPropertiesFromGLFW()) {
		PTGN_ERROR("NFD_SetDisplayPropertiesFromGLFW failed");
	}
}

FileDialog::~FileDialog() {
	NFD::Quit();
}

FileDialog::Result<path> FileDialog::OpenFile(const Options& options) const {
	return GetResult<NFD::UniquePathU8, path>(
		window_.instance_.get(), options,
		[](auto& out, auto filter_data, auto filter_count, auto default_path, auto,
		   auto native_window) {
			return NFD::OpenDialog(out, filter_data, filter_count, default_path, native_window);
		},
		[](auto from) { return path{ from.get() }; }
	);
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFiles(const Options& options) const {
	return GetResult<NFD::UniquePathSet, std::vector<path>>(
		window_.instance_.get(), options,
		[](auto& out, auto filter_data, auto filter_count, auto default_path, auto,
		   auto native_window) {
			return NFD::OpenDialogMultiple(
				out, filter_data, filter_count, default_path, native_window
			);
		},
		[](auto from) { return ToPaths(std::move(from)); }
	);
}

FileDialog::Result<path> FileDialog::SaveFile(const Options& options) const {
	return GetResult<NFD::UniquePathU8, path>(
		window_.instance_.get(), options,
		[](auto& out, auto filter_data, auto filter_count, auto default_path, auto default_name,
		   auto native_window) {
			return NFD::SaveDialog(
				out, filter_data, filter_count, default_path, default_name, native_window
			);
		},
		[](auto from) { return path{ from.get() }; }
	);
}

FileDialog::Result<path> FileDialog::OpenFolder(const Options& options) const {
	return GetResult<NFD::UniquePathU8, path>(
		window_.instance_.get(), options,
		[](auto& out, auto, auto, auto default_path, auto, auto native_window) {
			return NFD::PickFolder(out, default_path, native_window);
		},
		[](auto from) { return path{ from.get() }; }
	);
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFolders(const Options& options) const {
	return GetResult<NFD::UniquePathSet, std::vector<path>>(
		window_.instance_.get(), options,
		[](auto& out, auto, auto, auto default_path, auto, auto native_window) {
			return NFD::PickFolderMultiple(out, default_path, native_window);
		},
		[](auto from) { return ToPaths(std::move(from)); }
	);
}

} // namespace ptgn

#endif