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

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "core/log.h"

namespace ptgn {

FileDialog::FileDialog(Window& window) : window_{ window } {
	if (NFD_Init() != NFD_OKAY) {
		PTGN_ERROR("NFD_Init failed: ", NFD_GetError());
	}
	if (!NFD_SetDisplayPropertiesFromGLFW()) {
		PTGN_ERROR("NFD_SetDisplayPropertiesFromGLFW failed");
	}
}

FileDialog::~FileDialog() {
	NFD_Quit();
}

static std::string GetNFDError() {
	if (const char* err{ NFD_GetError() }) {
		return err;
	}
	return "Unknown NFD error";
}

struct NfdFilterStorage {
	std::vector<std::string> names;
	std::vector<std::string> specs;
	std::vector<nfdu8filteritem_t> items;
};

static NfdFilterStorage BuildFilters(const std::vector<FileDialog::Filter>& filters) {
	NfdFilterStorage out;
	out.names.reserve(filters.size());
	out.specs.reserve(filters.size());
	out.items.reserve(filters.size());

	for (const auto& filter : filters) {
		out.names.push_back(filter.name);
		out.specs.push_back(filter.spec);
	}

	for (std::size_t i = 0; i < filters.size(); ++i) {
		out.items.push_back(nfdu8filteritem_t{
			out.names[i].c_str(),
			out.specs[i].c_str(),
		});
	}

	return out;
}

struct DialogCommonData {
	NfdFilterStorage filters;
	std::string default_path;
	std::string default_name;
};

static DialogCommonData BuildDialogCommonData(const FileDialog::Options& options) {
	DialogCommonData data;
	data.filters = BuildFilters(options.filters);

	if (options.default_path.has_value()) {
		data.default_path = options.default_path->string();
	}

	if (options.default_name.has_value()) {
		data.default_name = *options.default_name;
	}

	return data;
}

template <typename TArgs>
static void FillCommonDialogArgs(
	GLFWwindow* glfw_window, const DialogCommonData& common, TArgs& args
) {
	if constexpr (requires {
					  args.filterList;
					  args.filterCount;
				  }) {
		args.filterList	 = common.filters.items.empty() ? nullptr : common.filters.items.data();
		args.filterCount = static_cast<nfdfiltersize_t>(common.filters.items.size());
	}

	if constexpr (requires { args.defaultPath; }) {
		args.defaultPath = common.default_path.empty() ? nullptr : common.default_path.c_str();
	}

	if constexpr (requires { args.defaultName; }) {
		args.defaultName = common.default_name.empty() ? nullptr : common.default_name.c_str();
	}

	NFD_GetNativeWindowFromGLFWWindow(glfw_window, &args.parentWindow);
}

static FileDialog::Result<path> MakeSinglePathResult(nfdresult_t res, nfdu8char_t* out_path) {
	switch (res) {
		case NFD_OKAY: {
			path result{ out_path ? out_path : "" };
			if (out_path) {
				NFD_FreePathU8(out_path);
			}
			return std::optional<path>{ std::move(result) };
		}
		case NFD_CANCEL: return std::optional<path>{ std::nullopt };
		case NFD_ERROR:	 return std::unexpected(GetNFDError());
		default:		 return std::unexpected("Unknown native file dialog result");
	}
}

static FileDialog::Result<std::vector<path>> MakePathSetResult(
	nfdresult_t res, const nfdpathset_t* out_paths
) {
	switch (res) {
		case NFD_OKAY: {
			std::vector<path> results;

			nfdpathsetsize_t count = 0;
			if (auto count_res{ NFD_PathSet_GetCount(out_paths, &count) }; count_res != NFD_OKAY) {
				NFD_PathSet_Free(out_paths);
				return std::unexpected(GetNFDError());
			}

			results.reserve(static_cast<std::size_t>(count));

			for (nfdpathsetsize_t i = 0; i < count; ++i) {
				nfdu8char_t* p = nullptr;
				if (auto path_res{ NFD_PathSet_GetPathU8(out_paths, i, &p) };
					path_res != NFD_OKAY) {
					NFD_PathSet_Free(out_paths);
					return std::unexpected(GetNFDError());
				}

				results.emplace_back(p ? p : "");
				if (p) {
					NFD_PathSet_FreePathU8(p);
				}
			}

			NFD_PathSet_Free(out_paths);
			return std::optional<std::vector<path>>{ std::move(results) };
		}
		case NFD_CANCEL: return std::optional<std::vector<path>>{ std::nullopt };
		case NFD_ERROR:	 return std::unexpected(GetNFDError());
		default:		 return std::unexpected("Unknown native file dialog result");
	}
}

template <typename TArgs, typename TOptions, typename TFunc>
static FileDialog::Result<path> RunSinglePathDialog(
	GLFWwindow* glfw_window, const TOptions& options, TFunc&& func
) {
	DialogCommonData common{ BuildDialogCommonData(options) };
	TArgs args{ nullptr };
	FillCommonDialogArgs(glfw_window, common, args);
	nfdu8char_t* out_path{ nullptr };
	return MakeSinglePathResult(func(&out_path, args), out_path);
}

template <typename TArgs, typename TOptions, typename TFunc>
static FileDialog::Result<std::vector<path>> RunPathSetDialog(
	GLFWwindow* glfw_window, const TOptions& options, TFunc&& func
) {
	DialogCommonData common{ BuildDialogCommonData(options) };
	TArgs args{ nullptr };
	FillCommonDialogArgs(glfw_window, common, args);
	const nfdpathset_t* out_paths{ nullptr };
	return MakePathSetResult(func(&out_paths, args), out_paths);
}

FileDialog::Result<path> FileDialog::OpenFile(const Options& options) const {
	return RunSinglePathDialog<nfdopendialogu8args_t>(
		window_.instance_.get(), options,
		[](nfdu8char_t** out_path, const nfdopendialogu8args_t& args) {
			return NFD_OpenDialogU8_With(out_path, &args);
		}
	);
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFiles(const Options& options) const {
	return RunPathSetDialog<nfdopendialogu8args_t>(
		window_.instance_.get(), options,
		[](const nfdpathset_t** out_paths, const nfdopendialogu8args_t& args) {
			return NFD_OpenDialogMultipleU8_With(out_paths, &args);
		}
	);
}

FileDialog::Result<path> FileDialog::SaveFile(const Options& options) const {
	return RunSinglePathDialog<nfdsavedialogu8args_t>(
		window_.instance_.get(), options,
		[](nfdu8char_t** out_path, const nfdsavedialogu8args_t& args) {
			return NFD_SaveDialogU8_With(out_path, &args);
		}
	);
}

FileDialog::Result<path> FileDialog::OpenFolder(const Options& options) const {
	return RunSinglePathDialog<nfdpickfolderu8args_t>(
		window_.instance_.get(), options,
		[](nfdu8char_t** out_path, const nfdpickfolderu8args_t& args) {
			return NFD_PickFolderU8_With(out_path, &args);
		}
	);
}

FileDialog::Result<std::vector<path>> FileDialog::OpenFolders(const Options& options) const {
	return RunPathSetDialog<nfdpickfolderu8args_t>(
		window_.instance_.get(), options,
		[](const nfdpathset_t** out_paths, const nfdpickfolderu8args_t& args) {
			return NFD_PickFolderMultipleU8_With(out_paths, &args);
		}
	);
}

} // namespace ptgn

#endif