#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "core/util/file.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_key.h"

namespace ptgn::editor {

class EditorContext;

bool AcceptAssetKeyDragDrop(
	AssetKey& value,
	std::optional<AssetKind> accepted_kind = std::nullopt
);

/// @return True once after an asset-key payload was accepted by any editor field.
[[nodiscard]] bool ConsumeAcceptedAssetKeyDrop();

template <typename T>
concept SpecificAssetKey =
	std::derived_from<std::remove_cvref_t<T>, AssetKey> &&
	!std::same_as<std::remove_cvref_t<T>, AssetKey> &&
	requires { std::remove_cvref_t<T>::kind; };

template <SpecificAssetKey T>
bool AcceptAssetKeyDragDrop(T& value) {
	using Value = std::remove_cvref_t<T>;
	return AcceptAssetKeyDragDrop(static_cast<AssetKey&>(value), Value::kind);
}

class ContentBrowserPanel {
public:
	enum class SortMode {
		Name,
		Type,
	};

	void OnRender(EditorContext& ctx);

private:
	void InitializePreviewIcons(EditorContext& ctx);
	void DrawContentBrowser(EditorContext& ctx);
	void DrawToolbar(EditorContext& ctx);
	void DrawFolderTree(EditorContext& ctx);
	void DrawAssetGrid(EditorContext& ctx);
	void DrawShaderConfiguration(EditorContext& ctx);

	void ImportFiles(EditorContext& ctx, const std::vector<path>& files);
	bool MoveAssetToFolder(EditorContext& ctx, const AssetKey& key, const path& folder);

	[[nodiscard]] std::vector<path> ConsumeDroppedFiles();
	void AddDroppedFile(const path& file_path);

	std::vector<path> dropped_files_;
	path selected_directory_;
	SortMode sort_mode_{ SortMode::Name };
	bool sort_ascending_{ true };
	std::string search_;
	std::string status_;
	std::string new_folder_name_;
	std::optional<ShaderKey> configuring_shader_;
	std::string selected_builtin_vertex_;
	std::string selected_builtin_fragment_;

	::ptgn::impl::TextureObject audio_icon_texture_;
	::ptgn::impl::TextureObject document_icon_texture_;
	bool preview_icons_initialized_{ false };
};

} // namespace ptgn::editor
