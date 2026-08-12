#include "panels/content_browser.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <format>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "content_browser_icons.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/surface.h"
#include "core/util/string.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "renderer/resources/shader.h"
#include "renderer/resources/texture_format.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font_system.h"
#include "runtime/scene/scene.h"
#include "tools/debug/debug_system.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

inline constexpr char kAssetKeyPayloadType[]{ "PTGN_ASSET_KEY" };
bool accepted_asset_key_drop{ false };
inline constexpr V2_int kEmbeddedIconSize{ 64, 64 };
inline constexpr float kFolderPaneWidth{ 220.0f };
inline constexpr float kTileTextHeight{ 92.0f };
inline constexpr int kMinItemsPerRow{ 1 };
inline constexpr int kMaxItemsPerRow{ 16 };

inline constexpr ImVec4 kSceneAssetBackground{ 0.13f, 0.23f, 0.38f, 1.0f };
inline constexpr ImVec4 kProjectAssetBackground{ 0.29f, 0.17f, 0.40f, 1.0f };
inline constexpr ImVec4 kResidentAssetBorder{ 0.88f, 0.72f, 0.20f, 1.0f };
inline constexpr ImVec4 kShaderErrorBorder{ 0.92f, 0.20f, 0.18f, 1.0f };

struct AssetKeyPayload {
	AssetKind kind{ AssetKind::Unknown };
	char key[256]{};
};

struct TilePreview {
	::ptgn::impl::TextureId texture;
	V2_int size;
	bool tint_with_text_color{ false };
};

void BeginAssetKeyDragDropSource(
	std::string_view key,
	AssetKind kind,
	std::optional<std::string_view> key_alias = std::nullopt
) {
	if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		return;
	}

	AssetKeyPayload payload;
	payload.kind = kind;
	const auto length{ std::min(key.size(), sizeof(payload.key) - 1) };
	std::memcpy(payload.key, key.data(), length);
	payload.key[length] = '\0';
	ImGui::SetDragDropPayload(kAssetKeyPayloadType, &payload, sizeof(payload));

	const auto text{ key_alias.value_or(key) };
	ImGui::TextUnformatted(text.data(), text.data() + text.size());
	ImGui::EndDragDropSource();
}

std::optional<AssetKeyPayload> AcceptAssetMovePayload() {
	if (!ImGui::BeginDragDropTarget()) {
		return std::nullopt;
	}

	std::optional<AssetKeyPayload> result;
	if (const auto* payload{ ImGui::AcceptDragDropPayload(kAssetKeyPayloadType) };
		payload && payload->DataSize == sizeof(AssetKeyPayload)) {
		result = *static_cast<const AssetKeyPayload*>(payload->Data);
	}

	ImGui::EndDragDropTarget();
	return result;
}

bool ContainsInsensitive(std::string_view value, std::string_view query) {
	return ToLower(std::string{ value }).contains(ToLower(std::string{ query }));
}

FileDialog::Options ImportOptions() {
	return FileDialog::Options{
		.filters = {
			{ .name = "Images", .spec = "png,jpg,jpeg,bmp,gif" },
			{ .name = "Audio", .spec = "ogg,mp3,wav,opus" },
			{ .name = "Fonts", .spec = "ttf,otf" },
			{ .name = "Shaders", .spec = "glsl" },
			{ .name = "Project Data", .spec = "json,ptgnprefab,ptgnscene" },
			{ .name = "All files", .spec = "*" },
		},
	};
}

std::string FormatBytes(std::uintmax_t bytes) {
	constexpr std::array units{ "B", "KB", "MB", "GB" };
	double value{ static_cast<double>(bytes) };
	std::size_t unit{ 0 };
	while (value >= 1024.0 && unit + 1 < units.size()) {
		value /= 1024.0;
		++unit;
	}
	return unit == 0 ? std::format("{} {}", bytes, units[unit])
					 : std::format("{:.2f} {}", value, units[unit]);
}

std::string FormatDuration(double seconds) {
	const auto total_seconds{ static_cast<std::uint64_t>(std::max(0.0, seconds)) };
	const auto minutes{ total_seconds / 60 };
	const auto remainder{ total_seconds % 60 };
	return std::format("{}:{:02}", minutes, remainder);
}

std::vector<path> GetChildDirectories(const path& directory) {
	std::vector<path> directories;
	std::error_code error;

	for (std::filesystem::directory_iterator it{ directory, error }, end;
		 !error && it != end;
		 it.increment(error)) {
		std::error_code entry_error;
		if (it->is_directory(entry_error) && !entry_error) {
			directories.emplace_back(it->path().filename());
		}
	}

	std::ranges::sort(directories);
	return directories;
}

std::string ShaderStageText(ShaderStageMask stages) {
	const bool vertex{ HasShaderStage(stages, ShaderStageMask::Vertex) };
	const bool fragment{ HasShaderStage(stages, ShaderStageMask::Fragment) };
	if (vertex && fragment) {
		return "Vertex + Fragment";
	}
	if (vertex) {
		return "Vertex";
	}
	if (fragment) {
		return "Fragment";
	}
	return "None detected";
}

::ptgn::impl::TextureObject CreateEmbeddedIconTexture(
	Renderer& renderer,
	std::span<const std::uint8_t> png
) {
	::ptgn::impl::Surface surface{ png, 4 };
	return ::ptgn::impl::RendererAccessor{ renderer }.CreateTexture(
		surface.Data(),
		TextureDesc{
			.size{ surface.GetSize() },
			.format = TextureFormat::RGBA8,
			.params{ TextureMinFilter::Linear, TextureMagFilter::Linear },
		}
	);
}

std::optional<TilePreview> GetTilePreview(
	const ::ptgn::impl::AssetRecord& asset,
	::ptgn::impl::TextureId audio_icon,
	::ptgn::impl::TextureId document_icon
) {
	if (asset.preview.has_value()) {
		return TilePreview{
			.texture = asset.preview->texture,
			.size = asset.preview->size,
		};
	}

	switch (asset.kind) {
		case AssetKind::Audio:
			return TilePreview{
				.texture = audio_icon,
				.size = kEmbeddedIconSize,
				.tint_with_text_color = true,
			};
		case AssetKind::Shader: [[fallthrough]];
		case AssetKind::Json: [[fallthrough]];
		case AssetKind::Prefab: [[fallthrough]];
		case AssetKind::Scene:
			return TilePreview{
				.texture = document_icon,
				.size = kEmbeddedIconSize,
				.tint_with_text_color = true,
			};
		case AssetKind::Texture: [[fallthrough]];
		case AssetKind::Font: [[fallthrough]];
		case AssetKind::Unknown: break;
	}
	return std::nullopt;
}

void DrawPreview(float preview_size, const std::optional<TilePreview>& preview) {
	const auto preview_min{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton("##preview", ImVec2{ preview_size, preview_size });
	const ImVec2 preview_max{ preview_min.x + preview_size, preview_min.y + preview_size };
	const auto& style{ ImGui::GetStyle() };
	auto* draw_list{ ImGui::GetWindowDrawList() };

	draw_list->AddRectFilled(
		preview_min,
		preview_max,
		ImGui::GetColorU32(ImGuiCol_FrameBg),
		style.FrameRounding
	);
	draw_list->AddRect(
		preview_min,
		preview_max,
		ImGui::GetColorU32(ImGuiCol_Border),
		style.FrameRounding
	);

	if (!preview.has_value() || !preview->texture || !preview->size.IsPositive()) {
		return;
	}

	constexpr float padding{ 6.0f };
	const float available{ std::max(1.0f, preview_size - padding * 2.0f) };
	const float source_width{ static_cast<float>(preview->size.x) };
	const float source_height{ static_cast<float>(preview->size.y) };
	const float scale{ std::min(available / source_width, available / source_height) };
	const ImVec2 image_size{ source_width * scale, source_height * scale };
	const ImVec2 image_min{
		preview_min.x + (preview_size - image_size.x) * 0.5f,
		preview_min.y + (preview_size - image_size.y) * 0.5f,
	};
	const ImVec2 image_max{ image_min.x + image_size.x, image_min.y + image_size.y };
	const auto tint{
		preview->tint_with_text_color ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
										 : ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f }
	};
	draw_list->AddImage(
		static_cast<ImTextureID>(preview->texture),
		image_min,
		image_max,
		ImVec2{ 0.0f, 0.0f },
		ImVec2{ 1.0f, 1.0f },
		ImGui::GetColorU32(tint)
	);
}

void DrawClippedText(std::string_view value, float width, bool disabled = false) {
	if (disabled) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}
	ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + std::max(1.0f, width));
	ImGui::TextUnformatted(value.data(), value.data() + value.size());
	ImGui::PopTextWrapPos();
	if (disabled) {
		ImGui::PopStyleColor();
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(value.size()), value.data());
	}
}

bool AssetLess(
	const ::ptgn::impl::AssetRecord& lhs,
	const ::ptgn::impl::AssetRecord& rhs,
	ContentBrowserPanel::SortMode mode
) {
	const auto lhs_type{ magic_enum::enum_name(lhs.kind) };
	const auto rhs_type{ magic_enum::enum_name(rhs.kind) };
	if (mode == ContentBrowserPanel::SortMode::Type && lhs_type != rhs_type) {
		return lhs_type < rhs_type;
	}
	if (lhs.key != rhs.key) {
		return lhs.key < rhs.key;
	}
	return lhs_type < rhs_type;
}

path AssetRelativePath(
	const ::ptgn::impl::AssetRecord& asset,
	const path& project_root,
	const path& assets_root
) {
	std::error_code error;
	const path absolute{
		asset.source_path.is_absolute() ? asset.source_path : project_root / asset.source_path
	};
	auto relative{ std::filesystem::relative(absolute, assets_root, error) };
	return error ? asset.source_path : relative.lexically_normal();
}

bool AssetInFolder(
	const ::ptgn::impl::AssetRecord& asset,
	const path& project_root,
	const path& assets_root,
	const path& selected_directory,
	bool recursive
) {
	const auto relative{ AssetRelativePath(asset, project_root, assets_root) };
	const auto parent{ relative.parent_path().lexically_normal() };
	const auto selected{ selected_directory.lexically_normal() };
	if (!recursive) {
		return parent == selected;
	}
	if (selected.empty() || selected == ".") {
		return true;
	}
	const auto parent_text{ parent.generic_string() };
	const auto selected_text{ selected.generic_string() };
	return parent_text == selected_text || parent_text.starts_with(selected_text + "/");
}

bool MatchesSearch(const ::ptgn::impl::AssetRecord& asset, std::string_view search) {
	if (search.empty()) {
		return true;
	}
	return ContainsInsensitive(asset.key.value, search) ||
		   ContainsInsensitive(magic_enum::enum_name(asset.kind), search) ||
		   ContainsInsensitive(asset.source_path.generic_string(), search);
}

void DrawMetadata(const ::ptgn::impl::AssetRecord& asset) {
	ImGui::Text("Type: %s", magic_enum::enum_name(asset.kind).data());
	ImGui::Text("State: %s", magic_enum::enum_name(asset.load_state).data());
	ImGui::Text("Size: %s", FormatBytes(asset.metadata.file_size).c_str());
	ImGui::Text("References: %zu", asset.reference_count);
	if (asset.globally_pinned) {
		ImGui::TextDisabled("Project preload asset");
	}
	if (asset.metadata.dimensions.has_value()) {
		ImGui::Text(
			"Dimensions: %d x %d",
			asset.metadata.dimensions->x,
			asset.metadata.dimensions->y
		);
	}
	if (asset.metadata.duration_seconds.has_value()) {
		ImGui::Text(
			"Length: %s",
			FormatDuration(asset.metadata.duration_seconds.value()).c_str()
		);
	}
	if (asset.kind == AssetKind::Shader) {
		ImGui::Text(
			"Stages: %s",
			ShaderStageText(asset.metadata.shader_stages).c_str()
		);
	}
	if (!asset.load_error.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.0f, 0.45f, 0.35f, 1.0f });
		ImGui::TextWrapped("%s", asset.load_error.c_str());
		ImGui::PopStyleColor();
	}
	ImGui::Separator();
}

bool DrawBuiltinCombo(
	const char* label,
	std::string& selected,
	std::span<const std::string> values
) {
	bool changed{ false };
	const char* preview{ selected.empty() ? "Select engine shader" : selected.c_str() };
	if (ImGui::BeginCombo(label, preview)) {
		for (const auto& value : values) {
			const bool active{ selected == value };
			if (ImGui::Selectable(value.c_str(), active)) {
				selected = value;
				changed = true;
			}
			if (active) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

std::string ShaderReferenceLabel(std::string_view reference) {
	if (reference.empty()) {
		return "Select source";
	}
	if (reference == kShaderSourceToken) {
		return "This shader file";
	}
	if (reference.starts_with(kBuiltinShaderPrefix)) {
		return "Engine / " + std::string{ reference.substr(kBuiltinShaderPrefix.size()) };
	}
	return "Project / " + std::string{ reference };
}

bool DrawShaderSourceCombo(
	const char* label,
	std::string& selected,
	ShaderStageMask requested_stage,
	const ::ptgn::impl::AssetRecord& owner,
	AssetManager& assets
) {
	bool changed{ false };
	const auto preview{ ShaderReferenceLabel(selected) };
	if (!ImGui::BeginCombo(label, preview.c_str())) {
		return false;
	}

	if (HasShaderStage(owner.metadata.shader_stages, requested_stage)) {
		const bool active{ selected == kShaderSourceToken };
		if (ImGui::Selectable("This shader file", active)) {
			selected = std::string{ kShaderSourceToken };
			changed = true;
		}
	}

	const auto engine_names{
		requested_stage == ShaderStageMask::Vertex
			? assets.GetEngineVertexShaderNames()
			: assets.GetEngineFragmentShaderNames()
	};
	if (ImGui::BeginMenu("Engine")) {
		for (const auto& name : engine_names) {
			const std::string value{ std::string{ kBuiltinShaderPrefix } + name };
			if (ImGui::MenuItem(name.c_str(), nullptr, selected == value)) {
				selected = value;
				changed = true;
			}
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu("Project")) {
		for (const auto& candidate : ::ptgn::impl::AssetAccessor{ assets }.GetAssets()) {
			if (candidate.kind != AssetKind::Shader || candidate.key == owner.key) {
				continue;
			}
			auto source{ assets.GetShaderSource(ShaderKey{ candidate.key }) };
			if (!source.has_value() || !HasShaderStage(DetectShaderStages(source.value()), requested_stage)) {
				continue;
			}
			auto serialized{ assets.GetCatalogAsset(candidate.key) };
			if (!serialized.has_value()) {
				continue;
			}
			const std::string value{ serialized->source_path.generic_string() };
			if (ImGui::MenuItem(candidate.key.value.c_str(), nullptr, selected == value)) {
				selected = value;
				changed = true;
			}
		}
		ImGui::EndMenu();
	}

	ImGui::EndCombo();
	return changed;
}

bool IsShaderDirectory(const path& directory) {
	return ToLower(directory.filename().string()) == "shaders";
}

} // namespace

bool AcceptAssetKeyDragDrop(AssetKey& value, std::optional<AssetKind> accepted_kind) {
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}

	bool changed{ false };
	const auto* payload{ ImGui::GetDragDropPayload() };
	if (payload && payload->IsDataType(kAssetKeyPayloadType) &&
		payload->DataSize == sizeof(AssetKeyPayload)) {
		const auto* asset_payload{ static_cast<const AssetKeyPayload*>(payload->Data) };
		if (!accepted_kind.has_value() || accepted_kind.value() == asset_payload->kind) {
			if (ImGui::AcceptDragDropPayload(kAssetKeyPayloadType)) {
				value.value = asset_payload->key;
				accepted_asset_key_drop = true;
				changed = true;
			}
		}
	}
	ImGui::EndDragDropTarget();
	return changed;
}


bool ConsumeAcceptedAssetKeyDrop() {
	const bool accepted{ accepted_asset_key_drop };
	accepted_asset_key_drop = false;
	return accepted;
}

std::vector<path> ContentBrowserPanel::ConsumeDroppedFiles() {
	auto files{ std::move(dropped_files_) };
	dropped_files_.clear();
	return files;
}

void ContentBrowserPanel::AddDroppedFile(const path& file_path) {
	dropped_files_.emplace_back(file_path);
}

void ContentBrowserPanel::InitializePreviewIcons(EditorContext& ctx) {
	if (preview_icons_initialized_) {
		return;
	}
	audio_icon_texture_ = CreateEmbeddedIconTexture(
		ctx.editor.GetRenderer(),
		std::span{ embedded::kAudioNotePng }
	);
	document_icon_texture_ = CreateEmbeddedIconTexture(
		ctx.editor.GetRenderer(),
		std::span{ embedded::kDocumentPng }
	);
	preview_icons_initialized_ = true;
}

void ContentBrowserPanel::OnRender(EditorContext& ctx) {
	if (ImGui::Begin("Content Browser")) {
		DrawContentBrowser(ctx);
	}
	ImGui::End();

	if (ImGui::Begin("Render Stats")) {
		auto& stats{ ctx.editor.GetDebugSystem().stats };
		const auto draw_calls{ "Draw calls: " + ToString(stats.GetCount("draw_calls")) };
		ImGui::TextUnformatted(draw_calls.c_str());
	}
	ImGui::End();

	DrawShaderConfiguration(ctx);
	DrawShaderEditor(ctx);
}

void ContentBrowserPanel::DrawContentBrowser(EditorContext& ctx) {
	InitializePreviewIcons(ctx);
	DrawToolbar(ctx);
	ImGui::Separator();

	if (!status_.empty()) {
		ImGui::TextDisabled("%s", status_.c_str());
		ImGui::Separator();
	}

	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
		auto files{ ConsumeDroppedFiles() };
		if (!files.empty()) {
			ImportFiles(ctx, files);
		}
	}

	if (ImGui::BeginTable(
			"##content_browser_layout",
			2,
			ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV
		)) {
		ImGui::TableSetupColumn(
			"Folders",
			ImGuiTableColumnFlags_WidthFixed,
			kFolderPaneWidth
		);
		ImGui::TableSetupColumn("Assets", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextColumn();
		DrawFolderTree(ctx);
		ImGui::TableNextColumn();
		DrawAssetGrid(ctx);
		ImGui::EndTable();
	}
}

void ContentBrowserPanel::DrawToolbar(EditorContext& ctx) {
	auto& settings{ ctx.editor.GetSettings() };
	settings.content_browser_items_per_row = std::clamp(
		settings.content_browser_items_per_row,
		kMinItemsPerRow,
		kMaxItemsPerRow
	);

	if (ImGui::Button("Import...")) {
		auto result{ ctx.editor.GetWindow().file.OpenFiles(ImportOptions()) };
		if (!result.has_value()) {
			status_ = "Import failed: " + result.error();
		} else if (result->has_value()) {
			ImportFiles(ctx, result->value());
		}
	}

	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		ctx.editor.GetAssetManager().RefreshCatalogFromDisk();
		ctx.editor.MarkProjectDirty();
		status_ = "Refreshed asset catalog";
	}

	ImGui::SameLine();
	if (ImGui::Button("New Folder")) {
		ImGui::OpenPopup("Create Asset Folder");
		new_folder_name_.clear();
	}

	if (ImGui::BeginPopupModal("Create Asset Folder", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::InputText("Name", &new_folder_name_);
		const bool valid{ !new_folder_name_.empty() && new_folder_name_ != "." &&
			new_folder_name_ != ".." &&
			new_folder_name_.find_first_of("/\\") == std::string::npos };
		ImGui::BeginDisabled(!valid);
		if (ImGui::Button("Create")) {
			if (const auto root{ ctx.editor.GetAssetManager().GetAssetDirectory() }) {
				EnsureDirectory(root.value() / selected_directory_ / new_folder_name_);
				status_ = "Created folder " + new_folder_name_;
			}
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	ImGui::SameLine();
	ImGui::TextUnformatted("Sort");
	ImGui::SameLine();
	ImGui::SetNextItemWidth(90.0f);
	const char* sort_name{ sort_mode_ == SortMode::Name ? "Name" : "Type" };
	if (ImGui::BeginCombo("##asset_sort", sort_name)) {
		if (ImGui::Selectable("Name", sort_mode_ == SortMode::Name)) {
			sort_mode_ = SortMode::Name;
		}
		if (ImGui::Selectable("Type", sort_mode_ == SortMode::Type)) {
			sort_mode_ = SortMode::Type;
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::ArrowButton("##asset_sort_direction", sort_ascending_ ? ImGuiDir_Up : ImGuiDir_Down)) {
		sort_ascending_ = !sort_ascending_;
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(sort_ascending_ ? "Ascending" : "Descending");
	}

	ImGui::SameLine();
	ImGui::Checkbox("All folders", &settings.content_browser_search_entire_tree);

	if (IsShaderDirectory(selected_directory_)) {
		ImGui::SameLine();
		ImGui::Checkbox("Show engine shaders", &show_engine_shaders_);
	}

	const float counter_width{ 58.0f };
	const float search_width{ std::max(120.0f, ImGui::GetContentRegionAvail().x - counter_width) };
	ImGui::SetNextItemWidth(search_width);
	ImGui::InputTextWithHint("##asset_search", "Search assets...", &search_);
	ImGui::SameLine();
	ImGui::Button(
		std::format("{} / row", settings.content_browser_items_per_row).c_str(),
		ImVec2{ counter_width, 0.0f }
	);
	if (ImGui::IsItemHovered()) {
		const float wheel{ ImGui::GetIO().MouseWheel };
		if (wheel != 0.0f) {
			settings.content_browser_items_per_row = std::clamp(
				settings.content_browser_items_per_row + (wheel > 0.0f ? 1 : -1),
				kMinItemsPerRow,
				kMaxItemsPerRow
			);
		}
		ImGui::SetTooltip("Hover and scroll to change items per row");
	}
}

bool ContentBrowserPanel::MoveAssetToFolder(
	EditorContext& ctx,
	const AssetKey& key,
	const path& folder
) {
	if (!ctx.editor.GetAssetManager().MoveAsset(key, folder)) {
		status_ = "Could not move " + key.value;
		return false;
	}
	ctx.editor.MarkProjectDirty();
	status_ = "Moved " + key.value + " to Assets/" + folder.generic_string();
	return true;
}

void ContentBrowserPanel::DrawFolderTree(EditorContext& ctx) {
	const auto assets_root{ ctx.editor.GetAssetManager().GetAssetDirectory() };
	if (!assets_root.has_value()) {
		ImGui::TextDisabled("Open a project to browse assets.");
		return;
	}

	ImGui::BeginChild("##asset_folder_tree", ImVec2{ 0.0f, 0.0f });

	const auto root_directories{ GetChildDirectories(assets_root.value()) };
	const bool root_has_children{ !root_directories.empty() };
	const bool root_selected{ selected_directory_.empty() };

	ImGuiTreeNodeFlags root_flags{ ImGuiTreeNodeFlags_SpanAvailWidth };
	if (root_has_children) {
		root_flags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow;
	} else {
		root_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
	}
	if (root_selected) {
		root_flags |= ImGuiTreeNodeFlags_Selected;
	}

	const bool root_open{ ImGui::TreeNodeEx("Assets", root_flags) };
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		selected_directory_.clear();
	}
	if (const auto payload{ AcceptAssetMovePayload() }) {
		MoveAssetToFolder(ctx, AssetKey{ payload->key }, {});
	}

	std::function<void(const path&, const std::vector<path>&)> draw_children;
	draw_children = [&](const path& relative_parent, const std::vector<path>& directories) {
		for (const auto& name : directories) {
			const path relative{ relative_parent / name };
			const auto child_directories{
				GetChildDirectories(assets_root.value() / relative)
			};
			const bool has_children{ !child_directories.empty() };

			ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth };
			if (has_children) {
				flags |= ImGuiTreeNodeFlags_OpenOnArrow;
			} else {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (selected_directory_.lexically_normal() == relative.lexically_normal()) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}

			const bool open{ ImGui::TreeNodeEx(name.string().c_str(), flags) };
			if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
				selected_directory_ = relative;
			}
			if (const auto payload{ AcceptAssetMovePayload() }) {
				MoveAssetToFolder(ctx, AssetKey{ payload->key }, relative);
			}
			if (has_children && open) {
				draw_children(relative, child_directories);
				ImGui::TreePop();
			}
		}
	};

	if (root_has_children && root_open) {
		draw_children({}, root_directories);
		ImGui::TreePop();
	}

	ImGui::EndChild();
}

void ContentBrowserPanel::ImportFiles(EditorContext& ctx, const std::vector<path>& files) {
	std::size_t imported{ 0 };
	std::size_t skipped{ 0 };
	for (const auto& file : files) {
		if (ctx.editor.GetAssetManager().ImportAsset(file, selected_directory_).has_value()) {
			++imported;
		} else {
			++skipped;
		}
	}
	if (imported > 0) {
		ctx.editor.MarkProjectDirty();
	}
	status_ = std::format("Imported {} asset{}", imported, imported == 1 ? "" : "s");
	if (skipped > 0) {
		status_ += std::format(", skipped {}", skipped);
	}
}

void ContentBrowserPanel::DrawAssetGrid(EditorContext& ctx) {
	auto& assets_manager{ ctx.editor.GetAssetManager() };
	const auto project_root{ assets_manager.GetProjectRoot() };
	const auto assets_root{ assets_manager.GetAssetDirectory() };
	if (!project_root.has_value() || !assets_root.has_value()) {
		ImGui::TextDisabled("No project asset directory.");
		return;
	}

	auto assets{ ::ptgn::impl::AssetAccessor{ assets_manager }.GetAssets() };
	const bool recursive{ ctx.editor.GetSettings().content_browser_search_entire_tree };
	std::erase_if(assets, [&](const ::ptgn::impl::AssetRecord& asset) {
		return !AssetInFolder(
			asset,
			project_root.value(),
			assets_root.value(),
			recursive ? path{} : selected_directory_,
			recursive
		) || !MatchesSearch(asset, search_);
	});

	if (show_engine_shaders_ && IsShaderDirectory(selected_directory_)) {
		auto engine_assets{ assets_manager.GetEngineShaderAssets() };
		std::erase_if(engine_assets, [&](const ::ptgn::impl::AssetRecord& asset) {
			return !MatchesSearch(asset, search_);
		});
		for (auto& engine_asset : engine_assets) {
			assets.emplace_back(std::move(engine_asset));
		}
	}

	std::ranges::stable_sort(assets, [&](const auto& lhs, const auto& rhs) {
		return sort_ascending_ ? AssetLess(lhs, rhs, sort_mode_)
							 : AssetLess(rhs, lhs, sort_mode_);
	});

	ImGui::BeginChild("##asset_grid", ImVec2{ 0.0f, 0.0f });
	if (assets.empty()) {
		ImGui::TextDisabled("No assets in this folder.");
		ImGui::EndChild();
		return;
	}

	const int columns{ std::clamp(
		ctx.editor.GetSettings().content_browser_items_per_row,
		kMinItemsPerRow,
		kMaxItemsPerRow
	) };
	Scene* selected_scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	std::optional<AssetKey> delete_asset;

	if (ImGui::BeginTable(
			"##asset_grid_table",
			columns,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX
		)) {
		for (int column{ 0 }; column < columns; ++column) {
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 1.0f);
		}

		for (const auto& asset : assets) {
			ImGui::TableNextColumn();
			ImGui::PushID(asset.key.value.c_str());
			const float width{ std::max(32.0f, ImGui::GetContentRegionAvail().x) };
			const float tile_height{ width + kTileTextHeight };
			const bool is_scene_asset{
				!asset.engine_asset && selected_scene && selected_scene->HasAssetDependency(asset.key)
			};
			const bool is_explicit_scene_asset{
				!asset.engine_asset && selected_scene && selected_scene->HasExplicitAssetDependency(asset.key)
			};
			const bool is_resident{ asset.load_state == AssetLoadState::Loaded };
			const bool draw_border{ asset.compile_error || is_resident };

			int pushed_colors{ 0 };
			if (asset.globally_pinned) {
				ImGui::PushStyleColor(ImGuiCol_ChildBg, kProjectAssetBackground);
				++pushed_colors;
			} else if (is_scene_asset) {
				ImGui::PushStyleColor(ImGuiCol_ChildBg, kSceneAssetBackground);
				++pushed_colors;
			}
			if (draw_border) {
				ImGui::PushStyleColor(
					ImGuiCol_Border,
					asset.compile_error ? kShaderErrorBorder : kResidentAssetBorder
				);
				++pushed_colors;
				ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 2.0f);
			}

			ImGui::BeginChild(
				"##asset_tile",
				ImVec2{ width, tile_height },
				true,
				ImGuiWindowFlags_NoScrollbar
			);

			const std::string label{
				asset.engine_asset
					? asset.source_path.filename().string()
					: (asset.key.value.empty() && asset.kind == AssetKind::Font
						   ? "Default Font"
						   : asset.key.value)
			};
			DrawPreview(
				width - ImGui::GetStyle().WindowPadding.x * 2.0f,
				GetTilePreview(
					asset,
					static_cast<::ptgn::impl::TextureId>(audio_icon_texture_),
					static_cast<::ptgn::impl::TextureId>(document_icon_texture_)
				)
			);
			if (!asset.engine_asset && asset.kind != AssetKind::Scene) {
				BeginAssetKeyDragDropSource(asset.key.value, asset.kind, label);
			}
			DrawClippedText(label, width);
			DrawClippedText(asset.engine_asset ? "Engine Shader" : magic_enum::enum_name(asset.kind), width, true);
			DrawClippedText(magic_enum::enum_name(asset.load_state), width, true);

			if (ImGui::BeginPopupContextWindow("##asset_context", ImGuiPopupFlags_MouseButtonRight)) {
				DrawMetadata(asset);
				if (asset.engine_asset) {
					ImGui::TextDisabled("Engine shader - read only");
					if (ImGui::MenuItem("View")) {
						OpenShaderEditor(ctx, asset);
					}
				} else {
					bool shader_ready{ true };
					if (asset.kind == AssetKind::Shader &&
						asset.metadata.shader_stages != ShaderStageMask::VertexFragment) {
						const auto catalog{ assets_manager.GetCatalogAsset(asset.key) };
						shader_ready = catalog.has_value() && catalog->shader.has_value() &&
							catalog->shader->vertex.has_value() && catalog->shader->fragment.has_value();
					}
					const bool can_pin{
						asset.cataloged && asset.kind != AssetKind::Scene && shader_ready && !asset.key.value.empty()
					};

					if (asset.manually_pinned) {
						if (ImGui::MenuItem("Remove from RAM")) {
							::ptgn::impl::AssetAccessor{ assets_manager }.Unload(asset.key, asset.kind);
							status_ = "Removed RAM pin from " + asset.key.value;
						}
					} else {
						ImGui::BeginDisabled(!can_pin);
						if (ImGui::MenuItem("Add to RAM")) {
							assets_manager.LoadAssetAsync(asset.key);
							status_ = "Loading " + asset.key.value;
						}
						ImGui::EndDisabled();
					}

					if (asset.globally_pinned) {
						if (ImGui::MenuItem("Remove from Project Preload")) {
							assets_manager.RemoveProjectAssetDependency(asset.key);
							ctx.editor.MarkProjectDirty();
						}
					} else {
						ImGui::BeginDisabled(!can_pin);
						if (ImGui::MenuItem("Add to Project Preload")) {
							assets_manager.PreloadProjectAsset(asset.key);
							ctx.editor.MarkProjectDirty();
						}
						ImGui::EndDisabled();
					}

					if (selected_scene && asset.kind != AssetKind::Scene) {
						const bool scene_editable{ !selected_scene->IsRuntime() && asset.cataloged };
						if (is_explicit_scene_asset) {
							ImGui::BeginDisabled(!scene_editable);
							if (ImGui::MenuItem("Remove from Selected Scene")) {
								if (selected_scene->RemoveAssetDependency(asset.key)) {
									selected_scene->SyncAssetDependenciesFromSerialization();
									ctx.editor.MarkProjectDirty();
								}
							}
							ImGui::EndDisabled();
						} else if (is_scene_asset) {
							ImGui::BeginDisabled();
							ImGui::MenuItem("Referenced by Selected Scene");
							ImGui::EndDisabled();
						} else {
							ImGui::BeginDisabled(!scene_editable);
							if (ImGui::MenuItem("Add to Selected Scene")) {
								if (selected_scene->AddAssetDependency(asset.key)) {
									selected_scene->SyncAssetDependenciesFromSerialization();
									ctx.editor.MarkProjectDirty();
								}
							}
							ImGui::EndDisabled();
						}
					}

					if (asset.kind == AssetKind::Shader) {
						ImGui::Separator();
						if (ImGui::MenuItem("Edit")) {
							OpenShaderEditor(ctx, asset);
						}
						if (ImGui::MenuItem("Configure Shader Program...")) {
							configuring_shader_ = ShaderKey{ asset.key };
							const auto catalog{ assets_manager.GetCatalogAsset(asset.key) };
							shader_separate_mode_ = catalog.has_value() && catalog->shader.has_value();
							selected_vertex_source_ = catalog && catalog->shader && catalog->shader->vertex
								? catalog->shader->vertex.value() : std::string{};
							selected_fragment_source_ = catalog && catalog->shader && catalog->shader->fragment
								? catalog->shader->fragment.value() : std::string{};
							ImGui::OpenPopup("Shader Program Configuration");
						}
					}

					ImGui::Separator();
					const bool can_delete{
						asset.cataloged && asset.kind != AssetKind::Scene && asset.reference_count == 0 &&
						!asset.globally_pinned && !asset.manually_pinned
					};
					ImGui::BeginDisabled(!can_delete);
					if (ImGui::MenuItem("Delete from Project")) {
						delete_asset = asset.key;
					}
					ImGui::EndDisabled();
				}
				ImGui::EndPopup();
			}

			ImGui::EndChild();
			if (draw_border) {
				ImGui::PopStyleVar();
			}
			if (pushed_colors > 0) {
				ImGui::PopStyleColor(pushed_colors);
			}
			if (!asset.engine_asset && asset.kind != AssetKind::Scene) {
				BeginAssetKeyDragDropSource(asset.key.value, asset.kind, label);
			}
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (delete_asset.has_value()) {
		if (assets_manager.DeleteAsset(delete_asset.value(), true)) {
			ctx.editor.MarkProjectDirty();
			status_ = "Deleted " + delete_asset->value;
		} else {
			status_ = "Could not delete " + delete_asset->value;
		}
	}
	ImGui::EndChild();
}

void ContentBrowserPanel::DrawShaderConfiguration(EditorContext& ctx) {
	if (!configuring_shader_.has_value()) {
		return;
	}

	ImGui::OpenPopup("Shader Program Configuration");
	bool keep_open{ true };
	if (ImGui::BeginPopupModal(
			"Shader Program Configuration",
			&keep_open,
			ImGuiWindowFlags_AlwaysAutoResize
		)) {
		auto& assets{ ctx.editor.GetAssetManager() };
		const auto records{ ::ptgn::impl::AssetAccessor{ assets }.GetAssets() };
		const auto it{ std::ranges::find_if(records, [&](const auto& record) {
			return record.key == static_cast<const AssetKey&>(configuring_shader_.value());
		}) };
		if (it == records.end()) {
			ImGui::TextDisabled("Shader no longer exists.");
		} else {
			ImGui::Text("Shader: %s", it->key.value.c_str());
			ImGui::Text("Detected in this file: %s", ShaderStageText(it->metadata.shader_stages).c_str());
			ImGui::Separator();

			if (ImGui::RadioButton("Combined file", !shader_separate_mode_)) {
				shader_separate_mode_ = false;
			}
			ImGui::SameLine();
			if (ImGui::RadioButton("Separate vertex / fragment", shader_separate_mode_)) {
				shader_separate_mode_ = true;
				if (selected_vertex_source_.empty() &&
					HasShaderStage(it->metadata.shader_stages, ShaderStageMask::Vertex)) {
					selected_vertex_source_ = std::string{ kShaderSourceToken };
				}
				if (selected_fragment_source_.empty() &&
					HasShaderStage(it->metadata.shader_stages, ShaderStageMask::Fragment)) {
					selected_fragment_source_ = std::string{ kShaderSourceToken };
				}
			}

			bool valid{ false };
			if (!shader_separate_mode_) {
				ImGui::TextWrapped("The current .glsl file supplies both stages.");
				valid = it->metadata.shader_stages == ShaderStageMask::VertexFragment;
				if (!valid) {
					ImGui::TextDisabled("Combined mode requires both #type vertex and #type fragment blocks.");
				}
			} else {
				DrawShaderSourceCombo("Vertex", selected_vertex_source_, ShaderStageMask::Vertex, *it, assets);
				DrawShaderSourceCombo("Fragment", selected_fragment_source_, ShaderStageMask::Fragment, *it, assets);
				valid = !selected_vertex_source_.empty() && !selected_fragment_source_.empty();
			}

			ImGui::Separator();
			ImGui::BeginDisabled(!valid);
			if (ImGui::Button("Save Configuration")) {
				const bool configured{
					shader_separate_mode_
						? assets.ConfigureShaderProgram(
							configuring_shader_.value(), selected_vertex_source_, selected_fragment_source_
						)
						: assets.ConfigureShaderProgram(configuring_shader_.value(), std::nullopt, std::nullopt)
				};
				if (configured) {
					ctx.editor.MarkProjectDirty();
					status_ = "Configured " + configuring_shader_->value + "; Reload to apply to a resident shader";
					configuring_shader_.reset();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				configuring_shader_.reset();
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}
	if (!keep_open) {
		configuring_shader_.reset();
	}
}


void ContentBrowserPanel::OpenShaderEditor(EditorContext& ctx, const ::ptgn::impl::AssetRecord& asset) {
	if (shader_editor_.has_value() && shader_editor_->key != ShaderKey{ asset.key } &&
		!shader_editor_->read_only && shader_editor_->source != shader_editor_->saved_source) {
		shader_editor_->open = true;
		status_ = "Save, discard, or close the current shader before opening another shader.";
		return;
	}
	if (shader_editor_.has_value() && shader_editor_->key == ShaderKey{ asset.key }) {
		shader_editor_->open = true;
		return;
	}

	auto source{ ctx.editor.GetAssetManager().GetShaderSource(ShaderKey{ asset.key }) };
	if (!source.has_value()) {
		status_ = "Could not read shader source for " + asset.key.value;
		return;
	}
	shader_editor_ = ShaderEditorState{
		.key = ShaderKey{ asset.key },
		.display_name = asset.engine_asset ? asset.source_path.filename().string() : asset.key.value,
		.source = source.value(),
		.saved_source = source.value(),
		.diagnostics = asset.compile_log,
		.last_compile_success = !asset.compile_error,
		.read_only = asset.read_only || asset.engine_asset,
		.open = true,
	};
}

bool ContentBrowserPanel::CanApplicationClose() {
	if (!shader_editor_.has_value() || shader_editor_->read_only ||
		shader_editor_->source == shader_editor_->saved_source) {
		return true;
	}
	application_close_requested_ = true;
	return false;
}

void ContentBrowserPanel::DrawShaderEditor(EditorContext& ctx) {
	if (!shader_editor_.has_value()) {
		return;
	}
	auto& editor{ shader_editor_.value() };
	auto& assets{ ctx.editor.GetAssetManager() };
	bool open{ editor.open };
	const std::string title{
		(editor.read_only ? "Shader Viewer - " : "Shader Editor - ") + editor.display_name +
		"###ShaderEditor"
	};
	if (ImGui::Begin(title.c_str(), &open, ImGuiWindowFlags_MenuBar)) {
		if (ImGui::BeginMenuBar()) {
			if (!editor.read_only) {
				if (ImGui::MenuItem("Save", "Ctrl+S")) {
					pending_shader_save_action_ = PendingShaderSaveAction::SaveOnly;
				}
				if (ImGui::MenuItem("Save + Recompile")) {
					pending_shader_save_action_ = PendingShaderSaveAction::SaveAndRecompile;
				}
				if (ImGui::MenuItem("Recompile Unsaved")) {
					auto result{ assets.RecompileShaderSource(editor.key, editor.source) };
					editor.last_compile_success = result.success;
					editor.diagnostics = std::move(result.log);
				}
			}
			ImGui::EndMenuBar();
		}

		if (editor.read_only) {
			ImGui::TextDisabled("Engine shader - read only");
		}
		const ImVec2 available{ ImGui::GetContentRegionAvail() };
		const float diagnostic_height{ std::clamp(available.y * 0.25f, 100.0f, 220.0f) };
		ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput };
		if (editor.read_only) {
			flags |= ImGuiInputTextFlags_ReadOnly;
		}
		ImGui::InputTextMultiline(
			"##ShaderSource",
			&editor.source,
			ImVec2{ -FLT_MIN, std::max(120.0f, available.y - diagnostic_height - 70.0f) },
			flags
		);

		if (!editor.read_only) {
			if (ImGui::Button("Save")) {
				pending_shader_save_action_ = PendingShaderSaveAction::SaveOnly;
			}
			ImGui::SameLine();
			if (ImGui::Button("Save + Recompile")) {
				pending_shader_save_action_ = PendingShaderSaveAction::SaveAndRecompile;
			}
			ImGui::SameLine();
			if (ImGui::Button("Recompile Unsaved")) {
				auto result{ assets.RecompileShaderSource(editor.key, editor.source) };
				editor.last_compile_success = result.success;
				editor.diagnostics = std::move(result.log);
			}
			ImGui::SameLine();
			ImGui::TextDisabled(editor.source == editor.saved_source ? "Saved" : "Unsaved changes");
		}

		ImGui::SeparatorText("Compiler Output");
		if (!editor.last_compile_success) {
			ImGui::PushStyleColor(ImGuiCol_Text, kShaderErrorBorder);
		}
		ImGui::BeginChild("##ShaderDiagnostics", ImVec2{ -FLT_MIN, diagnostic_height }, true);
		ImGui::TextWrapped("%s", editor.diagnostics.empty() ? "No compiler messages." : editor.diagnostics.c_str());
		ImGui::EndChild();
		if (!editor.last_compile_success) {
			ImGui::PopStyleColor();
		}
	}
	ImGui::End();

	if (!open && editor.open) {
		if (!editor.read_only && editor.source != editor.saved_source) {
			pending_shader_close_confirmation_ = true;
		} else {
			editor.open = false;
		}
	}

	auto perform_save = [&](PendingShaderSaveAction action, bool allow_invalid) {
		auto validation{ assets.ValidateShaderSource(editor.key, editor.source) };
		editor.last_compile_success = validation.success;
		editor.diagnostics = validation.log;
		if (!validation.success && !allow_invalid) {
			pending_shader_save_action_ = action;
			pending_shader_compile_error_confirmation_ = true;
			return;
		}
		if (!assets.SaveShaderSource(editor.key, editor.source, validation)) {
			editor.last_compile_success = false;
			editor.diagnostics += "\nCould not write shader file to disk.";
			return;
		}
		editor.saved_source = editor.source;
		if (validation.success && action == PendingShaderSaveAction::SaveAndRecompile) {
			auto result{ assets.RecompileShaderSource(editor.key, editor.source) };
			editor.last_compile_success = result.success;
			editor.diagnostics = std::move(result.log);
		}
		if (action == PendingShaderSaveAction::SaveAndClose) {
			editor.open = false;
		}
		if (action == PendingShaderSaveAction::SaveAndExit) {
			application_close_requested_ = false;
			ctx.editor.RequestQuit();
		}
	};

	if (pending_shader_save_action_ != PendingShaderSaveAction::None &&
		!pending_shader_compile_error_confirmation_) {
		const auto action{ pending_shader_save_action_ };
		pending_shader_save_action_ = PendingShaderSaveAction::None;
		perform_save(action, false);
	}

	if (pending_shader_compile_error_confirmation_) {
		ImGui::OpenPopup("Save Shader With Compile Errors?");
	}
	if (ImGui::BeginPopupModal("Save Shader With Compile Errors?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped(
			"This shader does not compile. Save it to disk anyway? The currently loaded "
			"shader will not be replaced."
		);
		if (ImGui::Button("Save Anyway")) {
			const auto action{ pending_shader_save_action_ };
			pending_shader_save_action_ = PendingShaderSaveAction::None;
			pending_shader_compile_error_confirmation_ = false;
			perform_save(action, true);
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			pending_shader_save_action_ = PendingShaderSaveAction::None;
			pending_shader_compile_error_confirmation_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (pending_shader_close_confirmation_) {
		ImGui::OpenPopup("Unsaved Shader Changes");
	}
	if (ImGui::BeginPopupModal("Unsaved Shader Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextWrapped("Save changes before closing this shader editor?");
		if (ImGui::Button("Save and Close")) {
			pending_shader_close_confirmation_ = false;
			pending_shader_save_action_ = PendingShaderSaveAction::SaveAndClose;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard")) {
			editor.open = false;
			pending_shader_close_confirmation_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			pending_shader_close_confirmation_ = false;
			editor.open = true;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (application_close_requested_) {
		ImGui::OpenPopup("Unsaved Shader Changes Before Exit");
	}
	if (ImGui::BeginPopupModal(
			"Unsaved Shader Changes Before Exit", nullptr, ImGuiWindowFlags_AlwaysAutoResize
		)) {
		ImGui::TextWrapped("The shader editor has unsaved changes. Save before exiting the application?");
		if (ImGui::Button("Save and Exit")) {
			application_close_requested_ = false;
			pending_shader_save_action_ = PendingShaderSaveAction::SaveAndExit;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Exit")) {
			editor.saved_source = editor.source;
			application_close_requested_ = false;
			ctx.editor.RequestQuit();
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Cancel")) {
			application_close_requested_ = false;
			ImGui::CloseCurrentPopup();
		}
		ImGui::EndPopup();
	}

	if (!editor.open) {
		shader_editor_.reset();
	}
}

} // namespace ptgn::editor
