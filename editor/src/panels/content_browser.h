#pragma once

#include <concepts>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "core/util/file.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/asset_serialization.h"

namespace ptgn::editor {

class EditorContext;

bool AcceptAssetKeyDragDrop(
	AssetKey& value,
	std::optional<AssetKind> accepted_kind = std::nullopt
);

/// @return True once after an asset-key payload was accepted by any editor field.
[[nodiscard]] bool ConsumeAcceptedAssetKeyDrop();

/// @brief Requests that the Content Browser open the shader editor for this key on its next render.
void RequestShaderEditorOpen(ShaderKey key);

struct ContentBrowserAssetSelection {
	AssetKey key;
	AssetKind kind{ AssetKind::Unknown };

	bool operator==(const ContentBrowserAssetSelection&) const = default;
};

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
	[[nodiscard]] bool CanApplicationClose();

	enum class SortMode {
		Name,
		Type,
	};

	void OnRender(EditorContext& ctx);

private:
	struct ShaderEditorState {
		ShaderKey key;
		std::string display_name;
		std::string vertex_source;
		std::string fragment_source;
		std::string saved_vertex_source;
		std::string saved_fragment_source;
		SerializedShaderProgram program;
		SerializedShaderProgram saved_program;
		ShaderStageMask source_stages{ ShaderStageMask::None };
		std::string diagnostics;
		bool last_compile_success{ true };
		bool vertex_editable{ false };
		bool fragment_editable{ false };
		bool read_only{ false };
		bool open{ true };
	};

	struct SelectionBoxState {
		float start_x{ 0.0f };
		float start_y{ 0.0f };
		std::vector<ContentBrowserAssetSelection> base_selection;
	};

	struct PendingDeleteState {
		std::vector<ContentBrowserAssetSelection> assets;
		std::optional<path> directory;
		std::vector<path> listed_files;
	};

	enum class PendingShaderSaveAction : std::uint8_t {
		None,
		SaveOnly,
		SaveAndRecompile,
		SaveAndClose,
		SaveAndExit,
	};

	void InitializePreviewIcons(EditorContext& ctx);
	void DrawContentBrowser(EditorContext& ctx);
	void DrawToolbar(EditorContext& ctx);
	void DrawFolderTree(EditorContext& ctx);
	void DrawAssetGrid(EditorContext& ctx);
	void DrawContentBrowserPopups(EditorContext& ctx);
	void DrawShaderEditor(EditorContext& ctx);
	void OpenShaderEditor(EditorContext& ctx, const impl::AssetRecord& asset);
	void OpenShaderEditor(EditorContext& ctx, const ShaderKey& key);

	void ImportFiles(EditorContext& ctx, const std::vector<path>& files);
	bool MoveSelectedAssets(EditorContext& ctx, const path& destination_directory);
	bool CreateDirectory(EditorContext& ctx, const path& parent_directory, std::string_view name);
	bool RenameDirectory(EditorContext& ctx, const path& directory, std::string_view new_name);
	bool DeleteDirectory(EditorContext& ctx, const path& directory);
	bool DeleteAssets(EditorContext& ctx, const std::vector<ContentBrowserAssetSelection>& assets);
	bool RenameAssetKey(EditorContext& ctx, ContentBrowserAssetSelection asset, std::string_view new_key);

	[[nodiscard]] std::vector<path> ConsumeDroppedFiles();
	void AddDroppedFile(const path& file_path);

	[[nodiscard]] bool IsAssetSelected(ContentBrowserAssetSelection asset) const;
	void SelectOnly(ContentBrowserAssetSelection asset);
	void ToggleSelection(ContentBrowserAssetSelection asset);
	void ClearAssetSelection();

	std::vector<path> dropped_files_;
	path selected_directory_;
	float folder_pane_width_{ 80.0f };
	bool folder_pane_manual_width_{ false };
	SortMode sort_mode_{ SortMode::Name };
	bool sort_ascending_{ true };
	std::string search_;
	std::string status_;
	bool show_engine_shaders_{ false };

	std::vector<ContentBrowserAssetSelection> selected_assets_;
	std::optional<SelectionBoxState> selection_box_;

	std::string new_folder_name_;
	std::optional<path> create_folder_parent_;
	std::optional<path> rename_directory_;
	std::string rename_directory_value_;
	std::optional<ContentBrowserAssetSelection> rename_asset_key_;
	std::string rename_asset_key_value_;
	std::optional<PendingDeleteState> pending_delete_;

	std::optional<ShaderEditorState> shader_editor_;
	PendingShaderSaveAction pending_shader_save_action_{ PendingShaderSaveAction::None };
	bool pending_shader_compile_error_confirmation_{ false };
	bool pending_shader_close_confirmation_{ false };
	bool application_close_requested_{ false };

	::ptgn::impl::TextureObject audio_icon_texture_;
	::ptgn::impl::TextureObject document_icon_texture_;
	bool preview_icons_initialized_{ false };
};

} // namespace ptgn::editor
