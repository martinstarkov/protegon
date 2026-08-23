#include "panels/content_browser.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "content_browser_icons.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/surface.h"
#include "core/util/hash.h"
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
#include "runtime/scene/scene_manager.h"
#include "tools/debug/debug_system.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

inline constexpr char kAssetKeyPayloadType[]{ "PTGN_ASSET_KEY" };
bool accepted_asset_key_drop{ false };
std::optional<ShaderKey> requested_shader_editor;
std::vector<ContentBrowserAssetSelection> dragged_asset_keys;
bool dragged_asset_move_allowed{ true };
inline constexpr std::string_view kBuiltinShaderDirectoryName{ "Built-In" };
inline constexpr V2_int kEmbeddedIconSize{ 64, 64 };
inline constexpr int kMinItemsPerRow{ 1 };
inline constexpr int kMaxItemsPerRow{ 16 };
inline constexpr std::array<std::pair<std::string_view, AssetKind>, 7> kAssetFilters{
	std::pair{ std::string_view{ "Audio" }, AssetKind::Audio },
	std::pair{ std::string_view{ "Data" }, AssetKind::Json },
	std::pair{ std::string_view{ "Fonts" }, AssetKind::Font },
	std::pair{ std::string_view{ "Prefabs" }, AssetKind::Prefab },
	std::pair{ std::string_view{ "Scenes" }, AssetKind::Scene },
	std::pair{ std::string_view{ "Shaders" }, AssetKind::Shader },
	std::pair{ std::string_view{ "Textures" }, AssetKind::Texture },
};

inline constexpr ImVec4 kSceneAssetBackground{ 0.13f, 0.23f, 0.38f, 1.0f };
inline constexpr ImVec4 kProjectAssetBackground{ 0.29f, 0.17f, 0.40f, 1.0f };
inline constexpr ImVec4 kResidentAssetBorder{ 0.20f, 0.55f, 0.95f, 1.0f };
inline constexpr ImVec4 kShaderErrorBorder{ 0.92f, 0.20f, 0.18f, 1.0f };
inline constexpr ImVec4 kSelectedAssetBorder{ 0.88f, 0.72f, 0.20f, 1.0f };

struct AssetKeyPayload {
	AssetKind kind{ AssetKind::Unknown };
	char key[256]{};
};

struct TilePreview {
	::ptgn::impl::TextureId texture{};
	V2_int size{};
	bool tint_with_text_color{ false };
};

struct AssetDeleteSnapshot {
	SerializedAsset asset{};
	std::vector<std::uint8_t> bytes{};
	bool project_preload{ false };
	bool manual_pin{ false };
};

struct DirectoryFileSnapshot {
	path relative_to_assets{};
	std::vector<std::uint8_t> bytes{};
	std::optional<SerializedAsset> asset{};
	bool project_preload{ false };
	bool manual_pin{ false };
};

struct DirectoryDeleteSnapshot {
	path directory{};
	std::vector<path> directories{};
	std::vector<DirectoryFileSnapshot> files{};
};

struct AssetTileRect {
	ContentBrowserAssetSelection asset{};
	ImVec2 min{};
	ImVec2 max{};
};

std::string AssetIdentityString(const AssetKey& key, AssetKind kind) {
	return std::to_string(std::to_underlying(kind)) + ":" + key.value;
}

void BeginAssetKeyDragDropSource(
	std::string_view key,
	AssetKind kind,
	std::optional<std::string_view> key_alias,
	std::span<const ContentBrowserAssetSelection> move_keys,
	bool move_allowed = true
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

	dragged_asset_move_allowed = move_allowed;
	dragged_asset_keys.clear();
	if (move_keys.empty()) {
		dragged_asset_keys.emplace_back(ContentBrowserAssetSelection{ AssetKey{ std::string{ key } }, kind });
	} else {
		dragged_asset_keys.assign(move_keys.begin(), move_keys.end());
	}

	if (dragged_asset_keys.size() > 1) {
		ImGui::Text("%zu assets", dragged_asset_keys.size());
	} else {
		const auto text{ key_alias.value_or(key) };
		ImGui::TextUnformatted(text.data(), text.data() + text.size());
	}
	ImGui::EndDragDropSource();
}

std::optional<std::vector<ContentBrowserAssetSelection>> AcceptAssetMovePayload() {
	if (!ImGui::BeginDragDropTarget()) {
		return std::nullopt;
	}

	std::optional<std::vector<ContentBrowserAssetSelection>> result;
	if (dragged_asset_move_allowed) {
		if (const auto* payload{ ImGui::AcceptDragDropPayload(kAssetKeyPayloadType) };
			payload && payload->DataSize == sizeof(AssetKeyPayload)) {
			if (!dragged_asset_keys.empty()) {
				result = dragged_asset_keys;
			} else {
				const auto* asset_payload{ static_cast<const AssetKeyPayload*>(payload->Data) };
				result = std::vector<ContentBrowserAssetSelection>{ ContentBrowserAssetSelection{ AssetKey{ asset_payload->key }, asset_payload->kind } };
			}
		}
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

void DrawPreviewAt(
	ImVec2 preview_min,
	float preview_size,
	const std::optional<TilePreview>& preview,
	ImU32 border_color
) {
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
		border_color,
		style.FrameRounding,
		0,
		border_color == ImGui::GetColorU32(ImGuiCol_Border) ? 1.0f : 2.0f
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
		preview->tint_with_text_color
			? ImGui::GetStyleColorVec4(ImGuiCol_Text)
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

bool MatchesSearch(const ::ptgn::impl::AssetRecord& asset, std::string_view search) {
	if (search.empty()) {
		return true;
	}
	return ContainsInsensitive(asset.key.value, search) ||
		   ContainsInsensitive(magic_enum::enum_name(asset.kind), search) ||
		   ContainsInsensitive(asset.source_path.generic_string(), search);
}

std::string AssetTypeText(const ::ptgn::impl::AssetRecord& asset) {
	if (asset.kind != AssetKind::Shader) {
		return std::string{ magic_enum::enum_name(asset.kind) };
	}

	const bool vertex{ HasShaderStage(asset.metadata.shader_stages, ShaderStageMask::Vertex) };
	const bool fragment{ HasShaderStage(asset.metadata.shader_stages, ShaderStageMask::Fragment) };
	if (vertex && fragment) {
		return "Vertex and Fragment Shader";
	}
	if (vertex) {
		return "Vertex Shader";
	}
	if (fragment) {
		return "Fragment Shader";
	}
	return "Shader";
}

void DrawMetadata(const ::ptgn::impl::AssetRecord& asset) {
	const auto extension_path{ asset.source_path.extension() };
	std::string extension{ extension_path.string() };
	if (!extension.empty() && extension.front() == '.') {
		extension.erase(extension.begin());
	}

	std::string name{ asset.source_path.stem().string() };
	if (name.empty()) {
		name = asset.key.value;
	}

	ImGui::Text("Type: %s", AssetTypeText(asset).c_str());
	ImGui::Text("Name: %s", name.empty() ? "Unknown" : name.c_str());
	ImGui::Text("Extension: %s", extension.empty() ? "None" : extension.c_str());
	ImGui::Text("Size: %s", FormatBytes(asset.metadata.file_size).c_str());
	ImGui::Text(
		"State: %s [%zu]",
		magic_enum::enum_name(asset.load_state).data(),
		asset.reference_count
	);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"The number in brackets is the count of live references currently retaining this asset."
		);
	}

	if (asset.metadata.dimensions.has_value()) {
		ImGui::Text(
			"Dimensions: %d x %d",
			asset.metadata.dimensions->x,
			asset.metadata.dimensions->y
		);
	}
	if (asset.metadata.duration_seconds.has_value()) {
		ImGui::Text("Length: %s", FormatDuration(asset.metadata.duration_seconds.value()).c_str());
	}
	if (asset.globally_pinned) {
		ImGui::TextDisabled("Project preload asset");
	}
	if (!asset.load_error.empty()) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.0f, 0.45f, 0.35f, 1.0f });
		ImGui::TextWrapped("%s", asset.load_error.c_str());
		ImGui::PopStyleColor();
	}
	ImGui::Separator();
}

std::string StripShaderStageDirective(std::string source) {
	const auto first{ source.find_first_not_of(" \t\r\n") };
	if (first == std::string::npos || source.compare(first, 6, "#type ") != 0) {
		return source;
	}

	const auto line_end{ source.find('\n', first) };
	if (line_end == std::string::npos) {
		return {};
	}

	source.erase(0, line_end + 1);
	return source;
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
			if (!source.has_value() ||
				!HasShaderStage(DetectShaderStages(source.value()), requested_stage)) {
				continue;
			}
			auto serialized{ assets.GetCatalogAsset(candidate.key, AssetKind::Shader) };
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

std::optional<AssetKind> AssetDirectoryKind(const path& directory) {
	if (directory.empty() || directory.is_absolute()) {
		return std::nullopt;
	}
	const auto normalized{ directory.lexically_normal() };
	const auto first{ normalized.begin() };
	if (first == normalized.end()) {
		return std::nullopt;
	}
	const auto name{ first->string() };
	const auto it{ std::ranges::find_if(kAssetFilters, [&](const auto& filter) {
		return filter.first == name;
	}) };
	return it == kAssetFilters.end() ? std::nullopt : std::optional<AssetKind>{ it->second };
}

bool IsBaseAssetDirectory(const path& directory) {
	if (!AssetDirectoryKind(directory).has_value()) {
		return false;
	}
	const auto normalized{ directory.lexically_normal() };
	return std::distance(normalized.begin(), normalized.end()) == 1;
}

path BuiltinShaderDirectory() {
	return path{ "Shaders" } / kBuiltinShaderDirectoryName;
}

bool IsBuiltinShaderDirectory(const path& directory) {
	return directory.lexically_normal() == BuiltinShaderDirectory();
}

bool IsUserAssetDirectory(const path& directory) {
	return AssetDirectoryKind(directory).has_value() &&
		!IsBaseAssetDirectory(directory) && !IsBuiltinShaderDirectory(directory);
}

bool IsShaderDirectory(const path& directory) {
	return AssetDirectoryKind(directory) == AssetKind::Shader;
}

std::vector<path> GetChildDirectories(const path& directory) {
	std::vector<path> directories;
	std::error_code error;
	for (std::filesystem::directory_iterator it{ directory, error }, end;
		 !error && it != end; it.increment(error)) {
		std::error_code entry_error;
		if (it->is_directory(entry_error) && !entry_error) {
			directories.emplace_back(it->path().filename());
		}
	}
	std::ranges::sort(directories);
	return directories;
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

bool AssetVisibleInDirectory(
	const ::ptgn::impl::AssetRecord& asset,
	const path& project_root,
	const path& assets_root,
	const path& selected_directory,
	bool recursive
) {
	if (asset.engine_asset) {
		return false;
	}
	if (!asset.cataloged && asset.kind == AssetKind::Font && asset.key.value == kDefaultFont) {
		const path selected{ selected_directory.lexically_normal() };
		return selected.empty() || selected == "." ||
			(IsBaseAssetDirectory(selected) && AssetDirectoryKind(selected) == AssetKind::Font);
	}
	const path relative{ AssetRelativePath(asset, project_root, assets_root) };
	const path parent{ relative.parent_path().lexically_normal() };
	const path selected{ selected_directory.lexically_normal() };
	if (selected.empty() || selected == ".") {
		return recursive;
	}
	if (!recursive) {
		return parent == selected;
	}
	if (parent == selected) {
		return true;
	}
	const auto parent_text{ parent.generic_string() };
	const auto selected_text{ selected.generic_string() };
	return parent_text.starts_with(selected_text + "/");
}

bool IsPathInsideDirectory(const path& candidate, const path& directory) {
	const auto normalized_candidate{ candidate.lexically_normal() };
	const auto normalized_directory{ directory.lexically_normal() };
	if (normalized_candidate == normalized_directory) {
		return true;
	}
	const auto relative{ normalized_candidate.lexically_relative(normalized_directory) };
	if (relative.empty() || relative.is_absolute()) {
		return false;
	}
	const auto first{ relative.begin() };
	return first != relative.end() && *first != "..";
}

bool ShaderProgramsEqual(
	const SerializedShaderProgram& lhs,
	const SerializedShaderProgram& rhs
) {
	return lhs.vertex == rhs.vertex && lhs.fragment == rhs.fragment;
}

SerializedShaderProgram ResolveShaderProgramForSource(
	AssetManager& assets,
	std::string_view source,
	const SerializedShaderProgram& current
) {
	const auto stages{ DetectShaderStages(source) };
	const auto suggested{ assets.SuggestShaderProgram(source).value_or(SerializedShaderProgram{}) };
	SerializedShaderProgram result;

	if (HasShaderStage(stages, ShaderStageMask::Vertex)) {
		result.vertex = std::string{ kShaderSourceToken };
	} else if (current.vertex.has_value() && current.vertex.value() != kShaderSourceToken) {
		result.vertex = current.vertex;
	} else {
		result.vertex = suggested.vertex;
	}

	if (HasShaderStage(stages, ShaderStageMask::Fragment)) {
		result.fragment = std::string{ kShaderSourceToken };
	} else if (current.fragment.has_value() && current.fragment.value() != kShaderSourceToken) {
		result.fragment = current.fragment;
	} else {
		result.fragment = suggested.fragment;
	}

	return result;
}

std::optional<std::vector<std::uint8_t>> ReadFileBytes(const path& file_path) {
	std::ifstream input{ file_path, std::ios::binary | std::ios::ate };
	if (!input) {
		return std::nullopt;
	}
	const auto end{ input.tellg() };
	if (end < 0) {
		return std::nullopt;
	}
	std::vector<std::uint8_t> bytes(static_cast<std::size_t>(end));
	input.seekg(0, std::ios::beg);
	if (!bytes.empty()) {
		input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	}
	return input ? std::optional<std::vector<std::uint8_t>>{ std::move(bytes) } : std::nullopt;
}

bool WriteFileBytes(const path& file_path, std::span<const std::uint8_t> bytes) {
	EnsureDirectory(file_path.parent_path());
	std::ofstream output{ file_path, std::ios::binary | std::ios::trunc };
	if (!output) {
		return false;
	}
	if (!bytes.empty()) {
		output.write(
			reinterpret_cast<const char*>(bytes.data()),
			static_cast<std::streamsize>(bytes.size())
		);
	}
	return static_cast<bool>(output);
}

bool IsDeletingAssetAllowed(const ::ptgn::impl::AssetRecord& record) {
	return record.cataloged && !record.engine_asset && record.kind != AssetKind::Scene &&
		record.reference_count == 0 && record.load_state != AssetLoadState::Queued &&
		record.load_state != AssetLoadState::Loading &&
		record.load_state != AssetLoadState::Finalizing;
}

std::unordered_map<std::string, ::ptgn::impl::AssetRecord> AssetRecordMap(AssetManager& assets) {
	std::unordered_map<std::string, ::ptgn::impl::AssetRecord> result;
	for (auto& record : ::ptgn::impl::AssetAccessor{ assets }.GetAssets()) {
		if (!record.engine_asset) {
			result.insert_or_assign(AssetIdentityString(record.key, record.kind), std::move(record));
		}
	}
	return result;
}

std::optional<std::vector<AssetDeleteSnapshot>> CaptureAssetDeleteSnapshots(
	AssetManager& assets,
	std::span<const ContentBrowserAssetSelection> keys
) {
	const auto project_root{ assets.GetProjectRoot() };
	if (!project_root.has_value()) {
		return std::nullopt;
	}
	const auto records{ AssetRecordMap(assets) };
	std::vector<AssetDeleteSnapshot> snapshots;
	snapshots.reserve(keys.size());

	for (const auto& selected : keys) {
		const auto record_it{ records.find(AssetIdentityString(selected.key, selected.kind)) };
		const auto serialized{ assets.GetCatalogAsset(selected.key, selected.kind) };
		if (record_it == records.end() || !serialized.has_value() ||
			!IsDeletingAssetAllowed(record_it->second)) {
			return std::nullopt;
		}
		const path absolute{
			serialized->source_path.is_absolute()
				? serialized->source_path
				: project_root.value() / serialized->source_path
		};
		auto bytes{ ReadFileBytes(absolute.lexically_normal()) };
		if (!bytes.has_value()) {
			return std::nullopt;
		}
		snapshots.emplace_back(AssetDeleteSnapshot{
			.asset = serialized.value(),
			.bytes = std::move(bytes.value()),
			.project_preload = record_it->second.globally_pinned,
			.manual_pin = record_it->second.manually_pinned,
		});
	}
	return snapshots;
}

bool DeleteAssetSnapshots(AssetManager& assets, std::span<const AssetDeleteSnapshot> snapshots) {
	const auto records{ AssetRecordMap(assets) };
	for (const auto& snapshot : snapshots) {
		const auto it{ records.find(AssetIdentityString(snapshot.asset.key, snapshot.asset.kind)) };
		if (it == records.end() || !IsDeletingAssetAllowed(it->second)) {
			return false;
		}
	}

	std::vector<const AssetDeleteSnapshot*> deleted;
	for (const auto& snapshot : snapshots) {
		if (snapshot.project_preload) {
			assets.RemoveProjectAssetDependency(snapshot.asset.key);
		}
		if (snapshot.manual_pin) {
			::ptgn::impl::AssetAccessor{ assets }.Unload(snapshot.asset.key, snapshot.asset.kind);
		}
		if (!assets.DeleteAsset(snapshot.asset.key, snapshot.asset.kind, true)) {
			for (const auto* restore : deleted) {
				const auto root{ assets.GetProjectRoot() };
				if (!root.has_value()) {
					continue;
				}
				const path absolute{ root.value() / restore->asset.source_path };
				WriteFileBytes(absolute, restore->bytes);
				if (assets.RestoreCatalogAsset(restore->asset)) {
					if (restore->project_preload) {
						assets.PreloadProjectAsset(restore->asset.key);
					}
					if (restore->manual_pin) {
						assets.LoadAssetAsync(restore->asset.key, restore->asset.kind);
					}
				}
			}
			return false;
		}
		deleted.push_back(&snapshot);
	}
	return true;
}

bool RestoreAssetSnapshots(AssetManager& assets, std::span<const AssetDeleteSnapshot> snapshots) {
	const auto root{ assets.GetProjectRoot() };
	if (!root.has_value()) {
		return false;
	}
	for (const auto& snapshot : snapshots) {
		const path absolute{
			snapshot.asset.source_path.is_absolute()
				? snapshot.asset.source_path
				: root.value() / snapshot.asset.source_path
		};
		if (!WriteFileBytes(absolute.lexically_normal(), snapshot.bytes)) {
			return false;
		}
		if (!assets.RestoreCatalogAsset(snapshot.asset)) {
			return false;
		}
		if (snapshot.project_preload) {
			assets.PreloadProjectAsset(snapshot.asset.key);
		}
		if (snapshot.manual_pin) {
			assets.LoadAssetAsync(snapshot.asset.key, snapshot.asset.kind);
		}
	}
	return true;
}

std::optional<DirectoryDeleteSnapshot> CaptureDirectoryDeleteSnapshot(
	AssetManager& assets,
	const path& relative_directory
) {
	const auto assets_root{ assets.GetAssetDirectory() };
	const auto project_root{ assets.GetProjectRoot() };
	if (!assets_root.has_value() || !project_root.has_value() ||
		!IsUserAssetDirectory(relative_directory)) {
		return std::nullopt;
	}

	const path absolute_directory{ (assets_root.value() / relative_directory).lexically_normal() };
	if (!IsDirectoryPath(absolute_directory.string())) {
		return std::nullopt;
	}

	DirectoryDeleteSnapshot snapshot{ .directory = relative_directory.lexically_normal() };
	snapshot.directories.emplace_back(snapshot.directory);

	const auto records{ AssetRecordMap(assets) };
	std::unordered_map<std::string, SerializedAsset> catalog_by_path;
	for (const auto& asset : assets.GetCatalog()) {
		const path absolute{
			asset.source_path.is_absolute() ? asset.source_path : project_root.value() / asset.source_path
		};
		catalog_by_path.insert_or_assign(absolute.lexically_normal().generic_string(), asset);
	}

	std::error_code error;
	for (std::filesystem::recursive_directory_iterator it{ absolute_directory, error }, end;
		 !error && it != end; it.increment(error)) {
		std::error_code entry_error;
		if (it->is_directory(entry_error) && !entry_error) {
			const auto relative{ std::filesystem::relative(it->path(), assets_root.value(), entry_error) };
			if (!entry_error) {
				snapshot.directories.emplace_back(relative.lexically_normal());
			}
			continue;
		}
		if (!it->is_regular_file(entry_error) || entry_error) {
			continue;
		}

		auto bytes{ ReadFileBytes(it->path()) };
		if (!bytes.has_value()) {
			return std::nullopt;
		}
		const auto relative_file{
			std::filesystem::relative(it->path(), assets_root.value(), entry_error)
		};
		if (entry_error) {
			return std::nullopt;
		}

		DirectoryFileSnapshot file{
			.relative_to_assets = relative_file.lexically_normal(),
			.bytes = std::move(bytes.value()),
		};
		const auto catalog_it{ catalog_by_path.find(it->path().lexically_normal().generic_string()) };
		if (catalog_it != catalog_by_path.end()) {
			const auto record_it{ records.find(AssetIdentityString(catalog_it->second.key, catalog_it->second.kind)) };
			if (record_it == records.end() || !IsDeletingAssetAllowed(record_it->second)) {
				return std::nullopt;
			}
			file.asset = catalog_it->second;
			file.project_preload = record_it->second.globally_pinned;
			file.manual_pin = record_it->second.manually_pinned;
		}
		snapshot.files.emplace_back(std::move(file));
	}
	return error ? std::nullopt : std::optional<DirectoryDeleteSnapshot>{ std::move(snapshot) };
}

bool DeleteDirectorySnapshot(AssetManager& assets, const DirectoryDeleteSnapshot& snapshot) {
	std::vector<AssetDeleteSnapshot> assets_to_delete;
	for (const auto& file : snapshot.files) {
		if (!file.asset.has_value()) {
			continue;
		}
		assets_to_delete.emplace_back(AssetDeleteSnapshot{
			.asset = file.asset.value(),
			.bytes = file.bytes,
			.project_preload = file.project_preload,
			.manual_pin = file.manual_pin,
		});
	}
	if (!DeleteAssetSnapshots(assets, assets_to_delete)) {
		return false;
	}

	const auto root{ assets.GetAssetDirectory() };
	if (!root.has_value()) {
		return false;
	}
	std::error_code error;
	std::filesystem::remove_all(root.value() / snapshot.directory, error);
	return !error;
}

bool RestoreDirectorySnapshot(AssetManager& assets, const DirectoryDeleteSnapshot& snapshot) {
	const auto assets_root{ assets.GetAssetDirectory() };
	if (!assets_root.has_value()) {
		return false;
	}
	for (const auto& directory : snapshot.directories) {
		EnsureDirectory(assets_root.value() / directory);
	}
	for (const auto& file : snapshot.files) {
		if (!WriteFileBytes(assets_root.value() / file.relative_to_assets, file.bytes)) {
			return false;
		}
	}
	for (const auto& file : snapshot.files) {
		if (!file.asset.has_value()) {
			continue;
		}
		if (!assets.RestoreCatalogAsset(file.asset.value())) {
			return false;
		}
		if (file.project_preload) {
			assets.PreloadProjectAsset(file.asset->key);
		}
		if (file.manual_pin) {
			assets.LoadAssetAsync(file.asset->key, file.asset->kind);
		}
	}
	return true;
}

bool AssetReferencedByEditor(EditorContext& ctx, const AssetKey& key) {
	for (const auto& scene : ctx.editor.GetSceneManager().GetScenes()) {
		if (scene && scene->HasAssetDependency(key)) {
			return true;
		}
	}
	if (const auto* effects{ ctx.editor.GetProjectScreenEffects() }) {
		json value = *effects;
		const auto dependencies{ ctx.editor.GetAssetManager().DiscoverDependencies(value) };
		if (std::ranges::contains(dependencies, key)) {
			return true;
		}
	}
	return false;
}

bool RectsOverlap(ImVec2 a_min, ImVec2 a_max, ImVec2 b_min, ImVec2 b_max) {
	return a_min.x <= b_max.x && a_max.x >= b_min.x &&
		a_min.y <= b_max.y && a_max.y >= b_min.y;
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

void RequestShaderEditorOpen(ShaderKey key) {
	requested_shader_editor = std::move(key);
}

std::vector<path> ContentBrowserPanel::ConsumeDroppedFiles() {
	auto files{ std::move(dropped_files_) };
	dropped_files_.clear();
	return files;
}

void ContentBrowserPanel::AddDroppedFile(const path& file_path) {
	dropped_files_.emplace_back(file_path);
}

bool ContentBrowserPanel::IsAssetSelected(ContentBrowserAssetSelection asset) const {
	return std::ranges::contains(selected_assets_, asset);
}

void ContentBrowserPanel::SelectOnly(ContentBrowserAssetSelection asset) {
	selected_assets_.assign(1, std::move(asset));
}

void ContentBrowserPanel::ToggleSelection(ContentBrowserAssetSelection asset) {
	if (const auto it{ std::ranges::find(selected_assets_, asset) }; it != selected_assets_.end()) {
		selected_assets_.erase(it);
	} else {
		selected_assets_.emplace_back(std::move(asset));
	}
}

void ContentBrowserPanel::ClearAssetSelection() {
	selected_assets_.clear();
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
	if (requested_shader_editor.has_value()) {
		OpenShaderEditor(ctx, requested_shader_editor.value());
		requested_shader_editor.reset();
	}

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

	DrawShaderEditor(ctx);
}

void ContentBrowserPanel::DrawContentBrowser(EditorContext& ctx) {
	InitializePreviewIcons(ctx);
	DrawToolbar(ctx);
	ImGui::Separator();


	if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows)) {
		auto files{ ConsumeDroppedFiles() };
		if (!files.empty()) {
			if (ctx.undo.IsUndoRedoEnabled()) {
				ImportFiles(ctx, files);
			} else {
				status_ = "Asset changes are disabled while undo/redo is unavailable.";
			}
		}
	}

	const float footer_height{
		ImGui::GetTextLineHeight() +
		ImGui::GetStyle().ItemSpacing.y * 2.0f +
		2.0f
	};
	const ImVec2 available{ ImGui::GetContentRegionAvail() };
	const float body_height{
		std::max(1.0f, available.y - footer_height)
	};

	ImGui::BeginChild(
		"##content_browser_body",
		ImVec2{ 0.0f, body_height },
		false,
		ImGuiWindowFlags_NoScrollbar
	);

	const ImVec2 body_available{ ImGui::GetContentRegionAvail() };
	constexpr float splitter_width{ 4.0f };
	const float max_folder_width{
		std::max(48.0f, body_available.x - splitter_width - 80.0f)
	};
	folder_pane_width_ = std::clamp(folder_pane_width_, 48.0f, max_folder_width);

	ImGui::BeginChild(
		"##content_browser_folders",
		ImVec2{ folder_pane_width_, body_available.y }
	);
	DrawFolderTree(ctx);
	ImGui::EndChild();

	ImGui::SameLine(0.0f, 0.0f);
	ImGui::InvisibleButton(
		"##content_browser_splitter",
		ImVec2{ splitter_width, std::max(1.0f, body_available.y) }
	);
	if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
		ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
	}
	if (ImGui::IsItemActive()) {
		folder_pane_manual_width_ = true;
		folder_pane_width_ = std::clamp(
			folder_pane_width_ + ImGui::GetIO().MouseDelta.x,
			48.0f,
			max_folder_width
		);
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
		folder_pane_manual_width_ = false;
	}
	const ImVec2 splitter_min{ ImGui::GetItemRectMin() };
	const ImVec2 splitter_max{ ImGui::GetItemRectMax() };
	ImGui::GetWindowDrawList()->AddLine(
		ImVec2{ (splitter_min.x + splitter_max.x) * 0.5f, splitter_min.y },
		ImVec2{ (splitter_min.x + splitter_max.x) * 0.5f, splitter_max.y },
		ImGui::GetColorU32(ImGuiCol_Border)
	);

	ImGui::SameLine(0.0f, 0.0f);
	DrawAssetGrid(ctx);


	ImGui::EndChild();

	ImGui::Separator();
	ImGui::TextDisabled("%s", status_.empty() ? " " : status_.c_str());

	DrawContentBrowserPopups(ctx);
}

void ContentBrowserPanel::DrawToolbar(EditorContext& ctx) {
	auto& settings{ ctx.editor.GetSettings() };
	settings.content_browser_items_per_row = std::clamp(
		settings.content_browser_items_per_row,
		kMinItemsPerRow,
		kMaxItemsPerRow
	);
	const bool can_mutate{ ctx.undo.IsUndoRedoEnabled() };

	const bool built_in_directory{ IsBuiltinShaderDirectory(selected_directory_) };
	ImGui::BeginDisabled(!can_mutate || built_in_directory);
	if (ImGui::Button("Import...")) {
		auto result{ ctx.editor.GetWindow().file.OpenFiles(ImportOptions()) };
		if (!result.has_value()) {
			status_ = "Import failed: " + result.error();
		} else if (result->has_value()) {
			ImportFiles(ctx, result->value());
		}
	}
	ImGui::SameLine();
	const bool can_create_folder{
		AssetDirectoryKind(selected_directory_).has_value() && !built_in_directory
	};
	ImGui::BeginDisabled(!can_create_folder);
	if (ImGui::Button("New Folder")) {
		create_folder_parent_ = selected_directory_;
		new_folder_name_.clear();
	}
	ImGui::EndDisabled();
	ImGui::EndDisabled();

	ImGui::SameLine();
	if (ImGui::Button("Refresh")) {
		ctx.editor.GetAssetManager().RefreshCatalogFromDisk();
		status_ = "Refreshed asset catalog";
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

	if (IsShaderDirectory(selected_directory_)) {
		ImGui::SameLine();
		if (ImGui::Checkbox("Show engine shaders", &show_engine_shaders_) &&
			!show_engine_shaders_ && IsBuiltinShaderDirectory(selected_directory_)) {
			selected_directory_ = path{ "Shaders" };
			ClearAssetSelection();
		}
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
	}
}

bool ContentBrowserPanel::CreateDirectory(
	EditorContext& ctx,
	const path& parent_directory,
	std::string_view name
) {
	if (!ctx.undo.IsUndoRedoEnabled() || !AssetDirectoryKind(parent_directory).has_value() ||
		IsBuiltinShaderDirectory(parent_directory) ||
		(IsBaseAssetDirectory(parent_directory) &&
		 AssetDirectoryKind(parent_directory) == AssetKind::Shader &&
		 name == kBuiltinShaderDirectoryName) ||
		name.empty() || name == "." || name == ".." ||
		name.find_first_of("/\\") != std::string_view::npos) {
		return false;
	}

	const auto root{ ctx.editor.GetAssetManager().GetAssetDirectory() };
	if (!root.has_value()) {
		return false;
	}

	const path relative{ (parent_directory / path{ name }).lexically_normal() };
	const path absolute{ (root.value() / relative).lexically_normal() };
	std::error_code error;
	if (std::filesystem::exists(absolute, error) || error) {
		return false;
	}

	std::filesystem::create_directories(absolute, error);
	if (error || !std::filesystem::is_directory(absolute, error) || error) {
		return false;
	}

	ctx.undo.PushApplied(
		"Create Asset Directory",
		[absolute]() {
			std::error_code remove_error;
			std::filesystem::remove(absolute, remove_error);
		},
		[absolute]() {
			std::error_code create_error;
			std::filesystem::create_directories(absolute, create_error);
		},
		false
	);
	status_ = "Created Assets/" + relative.generic_string();
	return true;
}

bool ContentBrowserPanel::RenameDirectory(
	EditorContext& ctx,
	const path& directory,
	std::string_view new_name
) {
	if (!ctx.undo.IsUndoRedoEnabled() || !IsUserAssetDirectory(directory) || new_name.empty() ||
		new_name == "." || new_name == ".." ||
		new_name.find_first_of("/\\") != std::string_view::npos) {
		return false;
	}
	const path before{ directory.lexically_normal() };
	const path after{ (before.parent_path() / path{ new_name }).lexically_normal() };
	if (before == after) {
		return true;
	}
	auto* assets{ &ctx.editor.GetAssetManager() };
	if (!assets->MoveAssetDirectory(before, after)) {
		return false;
	}
	if (selected_directory_ == before || IsPathInsideDirectory(selected_directory_, before)) {
		const path tail{ selected_directory_.lexically_relative(before) };
		selected_directory_ = (after / tail).lexically_normal();
	}
	ctx.undo.PushApplied(
		"Rename Asset Directory",
		[assets, before, after]() { assets->MoveAssetDirectory(after, before); },
		[assets, before, after]() { assets->MoveAssetDirectory(before, after); }
	);
	status_ = "Renamed Assets/" + before.generic_string() + " to Assets/" + after.generic_string();
	return true;
}

bool ContentBrowserPanel::MoveSelectedAssets(EditorContext& ctx, const path& destination_directory) {
	if (!ctx.undo.IsUndoRedoEnabled() || selected_assets_.empty()) {
		return false;
	}

	if (IsBuiltinShaderDirectory(destination_directory)) {
		status_ = "Built-In is a read-only virtual shader directory.";
		return false;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	const auto destination_kind{ AssetDirectoryKind(destination_directory) };
	const auto project_root{ assets.GetProjectRoot() };
	const auto assets_root{ assets.GetAssetDirectory() };
	if (!destination_kind.has_value() || !project_root.has_value() || !assets_root.has_value()) {
		return false;
	}

	std::vector<std::pair<ContentBrowserAssetSelection, path>> before;
	before.reserve(selected_assets_.size());
	for (const auto& selected : selected_assets_) {
		const auto asset{ assets.GetCatalogAsset(selected.key, selected.kind) };
		if (!asset.has_value() || asset->kind != destination_kind.value()) {
			status_ = "All moved assets must belong to the destination type.";
			return false;
		}

		const path absolute{
			asset->source_path.is_absolute()
				? asset->source_path
				: project_root.value() / asset->source_path
		};
		std::error_code error;
		const auto relative{ std::filesystem::relative(absolute, assets_root.value(), error) };
		if (error) {
			return false;
		}
		before.emplace_back(selected, relative.parent_path().lexically_normal());
	}

	std::size_t moved{ 0 };
	for (const auto& [selected, old_directory] : before) {
		if (!assets.MoveAsset(selected.key, selected.kind, destination_directory)) {
			for (std::size_t i{ 0 }; i < moved; ++i) {
				assets.MoveAsset(before[i].first.key, before[i].first.kind, before[i].second);
			}
			status_ = "Could not move the selected assets. A destination filename may already exist or the filesystem move may have failed.";
			return false;
		}
		++moved;
	}

	auto* assets_ptr{ &assets };
	const path destination{ destination_directory.lexically_normal() };
	ctx.undo.PushApplied(
		before.size() == 1 ? "Move Asset" : "Move Assets",
		[assets_ptr, before]() {
			for (const auto& [selected, directory] : before) {
				assets_ptr->MoveAsset(selected.key, selected.kind, directory);
			}
		},
		[assets_ptr, selected = selected_assets_, destination]() {
			for (const auto& asset : selected) {
				assets_ptr->MoveAsset(asset.key, asset.kind, destination);
			}
		}
	);

	status_ = std::format(
		"Moved {} asset{} to Assets/{}",
		before.size(),
		before.size() == 1 ? "" : "s",
		destination.generic_string()
	);
	return true;
}

bool ContentBrowserPanel::DeleteAssets(
	EditorContext& ctx,
	const std::vector<ContentBrowserAssetSelection>& selected
) {
	if (!ctx.undo.IsUndoRedoEnabled() || selected.empty()) {
		return false;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	auto captured{ CaptureAssetDeleteSnapshots(assets, selected) };
	if (!captured.has_value()) {
		status_ = "One or more selected assets cannot be deleted while in use or loading.";
		return false;
	}

	auto snapshots{
		std::make_shared<std::vector<AssetDeleteSnapshot>>(std::move(captured.value()))
	};
	if (!DeleteAssetSnapshots(assets, *snapshots)) {
		status_ = "Could not delete the selected assets.";
		return false;
	}

	auto* assets_ptr{ &assets };
	ctx.undo.PushApplied(
		selected.size() == 1 ? "Delete Asset" : "Delete Assets",
		[assets_ptr, snapshots]() { RestoreAssetSnapshots(*assets_ptr, *snapshots); },
		[assets_ptr, snapshots]() { DeleteAssetSnapshots(*assets_ptr, *snapshots); }
	);
	for (const auto& asset : selected) {
		std::erase(selected_assets_, asset);
	}
	status_ = std::format(
		"Deleted {} asset{}",
		selected.size(),
		selected.size() == 1 ? "" : "s"
	);
	return true;
}

bool ContentBrowserPanel::DeleteDirectory(EditorContext& ctx, const path& directory) {
	if (!ctx.undo.IsUndoRedoEnabled() || !IsUserAssetDirectory(directory)) {
		return false;
	}
	auto& assets{ ctx.editor.GetAssetManager() };
	auto captured{ CaptureDirectoryDeleteSnapshot(assets, directory) };
	if (!captured.has_value()) {
		status_ = "The directory contains a scene or an asset that is currently in use/loading.";
		return false;
	}
	auto snapshot{ std::make_shared<DirectoryDeleteSnapshot>(std::move(captured.value())) };
	if (!DeleteDirectorySnapshot(assets, *snapshot)) {
		status_ = "Could not delete Assets/" + directory.generic_string();
		return false;
	}
	const path parent{ directory.parent_path() };
	if (selected_directory_ == directory || IsPathInsideDirectory(selected_directory_, directory)) {
		selected_directory_ = parent;
	}
	auto* assets_ptr{ &assets };
	ctx.undo.PushApplied(
		"Delete Asset Directory",
		[assets_ptr, snapshot]() { RestoreDirectorySnapshot(*assets_ptr, *snapshot); },
		[assets_ptr, snapshot]() { DeleteDirectorySnapshot(*assets_ptr, *snapshot); }
	);
	ClearAssetSelection();
	status_ = "Deleted Assets/" + directory.generic_string();
	return true;
}

bool ContentBrowserPanel::RenameAssetKey(
	EditorContext& ctx,
	ContentBrowserAssetSelection asset,
	std::string_view new_key
) {
	if (!ctx.undo.IsUndoRedoEnabled() || new_key.empty() || new_key.contains('\0') ||
		(asset.kind == AssetKind::Font && asset.key.value == kDefaultFont) ||
		AssetReferencedByEditor(ctx, asset.key)) {
		status_ = "Asset keys can only be renamed while they are not referenced by an editor scene or screen effect.";
		return false;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	const AssetKey before{ asset.key };
	const AssetKey after{ std::string{ new_key } };
	if (before == after) {
		return true;
	}
	if (!assets.RenameAssetKey(before, asset.kind, after)) {
		status_ = "Could not rename the asset key. The new key may already exist for this asset type or the asset may be resident/in use.";
		return false;
	}

	for (auto& selected : selected_assets_) {
		if (selected == asset) {
			selected.key = after;
		}
	}

	auto* assets_ptr{ &assets };
	ctx.undo.PushApplied(
		"Rename Asset Key",
		[assets_ptr, kind = asset.kind, before, after]() {
			assets_ptr->RenameAssetKey(after, kind, before);
		},
		[assets_ptr, kind = asset.kind, before, after]() {
			assets_ptr->RenameAssetKey(before, kind, after);
		}
	);
	status_ = "Renamed " + before.value + " to " + after.value;
	return true;
}

void ContentBrowserPanel::ImportFiles(EditorContext& ctx, const std::vector<path>& files) {
	if (!ctx.undo.IsUndoRedoEnabled()) {
		status_ = "Asset changes are disabled while undo/redo is unavailable.";
		return;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	std::unordered_set<std::string> existing;
	for (const auto& asset : assets.GetCatalog()) {
		existing.emplace(AssetIdentityString(asset.key, asset.kind));
	}

	std::vector<ContentBrowserAssetSelection> imported;
	for (const auto& file : files) {
		auto key{ assets.ImportAsset(file, selected_directory_) };
		if (!key.has_value()) {
			continue;
		}

		for (const auto& asset : assets.GetCatalog()) {
			if (asset.key != key.value()) {
				continue;
			}
			const auto identity{ AssetIdentityString(asset.key, asset.kind) };
			if (existing.insert(identity).second) {
				imported.emplace_back(ContentBrowserAssetSelection{ asset.key, asset.kind });
			}
		}
	}

	if (imported.empty()) {
		status_ = "No new assets were imported.";
		return;
	}

	auto snapshots_optional{ CaptureAssetDeleteSnapshots(assets, imported) };
	if (!snapshots_optional.has_value()) {
		status_ = "Imported assets, but could not create an undo snapshot.";
		return;
	}

	auto snapshots{
		std::make_shared<std::vector<AssetDeleteSnapshot>>(std::move(snapshots_optional.value()))
	};
	auto* assets_ptr{ &assets };
	ctx.undo.PushApplied(
		imported.size() == 1 ? "Import Asset" : "Import Assets",
		[assets_ptr, snapshots]() { DeleteAssetSnapshots(*assets_ptr, *snapshots); },
		[assets_ptr, snapshots]() { RestoreAssetSnapshots(*assets_ptr, *snapshots); }
	);
	selected_assets_ = imported;
	status_ = std::format(
		"Imported {} asset{}",
		imported.size(),
		imported.size() == 1 ? "" : "s"
	);
}

void ContentBrowserPanel::DrawFolderTree(EditorContext& ctx) {
	const auto assets_root{ ctx.editor.GetAssetManager().GetAssetDirectory() };
	if (!assets_root.has_value()) {
		ImGui::TextDisabled("Open a project to browse assets.");
		return;
	}

	const auto& style{ ImGui::GetStyle() };

	const float tree_start_x{ ImGui::GetCursorScreenPos().x };
	const float leading_padding{
		std::max(0.0f, tree_start_x - ImGui::GetWindowPos().x)
	};
	const float trailing_padding{ std::max(3.0f, style.FramePadding.x) };
	float measured_width{
		leading_padding +
		ImGui::GetTreeNodeToLabelSpacing() +
		ImGui::CalcTextSize("Assets").x +
		trailing_padding
	};

	auto measure_node = [&](std::string_view label) {
		const float indent{
			std::max(
				0.0f,
				ImGui::GetCursorScreenPos().x - tree_start_x
			)
		};
		measured_width = std::max(
			measured_width,
			leading_padding +
				indent +
				ImGui::GetTreeNodeToLabelSpacing() +
				ImGui::CalcTextSize(label.data(), label.data() + label.size()).x +
				trailing_padding
		);
	};

	ImGuiTreeNodeFlags root_flags{
		ImGuiTreeNodeFlags_DefaultOpen |
		ImGuiTreeNodeFlags_OpenOnArrow |
		ImGuiTreeNodeFlags_SpanFullWidth
	};
	if (selected_directory_.empty()) {
		root_flags |= ImGuiTreeNodeFlags_Selected;
	}

	measure_node("Assets");
	const bool root_open{ ImGui::TreeNodeEx("Assets", root_flags) };
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
		selected_directory_.clear();
		ClearAssetSelection();
	}

	std::function<void(const path&)> draw_directory;
	draw_directory = [&](const path& relative) {
		const std::string id{ relative.lexically_normal().generic_string() };
		ImGui::PushID(id.c_str());

		const bool built_in_directory{ IsBuiltinShaderDirectory(relative) };
		const path absolute{ assets_root.value() / relative };
		auto children{
			built_in_directory ? std::vector<path>{} : GetChildDirectories(absolute)
		};
		if (IsBaseAssetDirectory(relative) && AssetDirectoryKind(relative) == AssetKind::Shader) {
			std::erase_if(children, [](const path& child) {
				return child.filename().string() == kBuiltinShaderDirectoryName;
			});
		}
		const bool has_virtual_builtin{
			show_engine_shaders_ &&
			IsBaseAssetDirectory(relative) &&
			AssetDirectoryKind(relative) == AssetKind::Shader
		};
		const bool has_children{ !children.empty() || has_virtual_builtin };

		ImGuiTreeNodeFlags flags{
			ImGuiTreeNodeFlags_SpanFullWidth |
			ImGuiTreeNodeFlags_OpenOnArrow
		};
		if (!has_children) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (selected_directory_.lexically_normal() == relative.lexically_normal()) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		const std::string name{ relative.filename().string() };
		measure_node(name);
		const bool open{ ImGui::TreeNodeEx("##Directory", flags, "%s", name.c_str()) };

		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) {
			selected_directory_ = relative;
			ClearAssetSelection();
		}

		if (!built_in_directory) {
			ImGui::OpenPopupOnItemClick(
				"##DirectoryContext",
				ImGuiPopupFlags_MouseButtonRight
			);

			if (const auto move{ AcceptAssetMovePayload() };
				move.has_value() && ctx.undo.IsUndoRedoEnabled()) {
				selected_assets_ = move.value();
				MoveSelectedAssets(ctx, relative);
			}

			if (ImGui::BeginPopup("##DirectoryContext")) {
				ImGui::BeginDisabled(!ctx.undo.IsUndoRedoEnabled());

				if (ImGui::MenuItem("New Folder")) {
					create_folder_parent_ = relative;
					new_folder_name_.clear();
				}

				if (IsUserAssetDirectory(relative)) {
					if (ImGui::MenuItem("Rename...")) {
						rename_directory_ = relative;
						rename_directory_value_ = relative.filename().string();
					}
					if (ImGui::MenuItem("Delete...")) {
						pending_delete_ = PendingDeleteState{ .directory = relative };
					}
				}

				ImGui::EndDisabled();
				ImGui::EndPopup();
			}
		}

		if (has_children && open) {
			for (const auto& child : children) {
				draw_directory(relative / child);
			}
			if (has_virtual_builtin) {
				draw_directory(BuiltinShaderDirectory());
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	};

	if (root_open) {
		const float base_unindent{ std::max(0.0f, style.IndentSpacing - 6.0f) };
		ImGui::Unindent(base_unindent);
		for (const auto& [name, kind] : kAssetFilters) {
			(void)kind;
			draw_directory(path{ name });
		}
		ImGui::Indent(base_unindent);
		ImGui::TreePop();
	}

	if (!folder_pane_manual_width_) {
		folder_pane_width_ = std::ceil(measured_width);
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

	const bool built_in_directory{ IsBuiltinShaderDirectory(selected_directory_) };
	const bool recursive{
		selected_directory_.empty() ||
		IsBaseAssetDirectory(selected_directory_)
	};
	auto assets{ ::ptgn::impl::AssetAccessor{ assets_manager }.GetAssets() };
	if (built_in_directory) {
		assets.clear();
	} else {
		std::erase_if(assets, [&](const ::ptgn::impl::AssetRecord& asset) {
			return !AssetVisibleInDirectory(
				asset,
				project_root.value(),
				assets_root.value(),
				selected_directory_,
				recursive
			) || !MatchesSearch(asset, search_);
		});
	}

	if (show_engine_shaders_ && built_in_directory) {
		auto engine_assets{ assets_manager.GetEngineShaderAssets() };
		std::erase_if(engine_assets, [&](const ::ptgn::impl::AssetRecord& asset) {
			return !MatchesSearch(asset, search_);
		});
		for (auto& engine_asset : engine_assets) {
			assets.emplace_back(std::move(engine_asset));
		}
	}

	std::ranges::stable_sort(assets, [&](const auto& lhs, const auto& rhs) {
		return sort_ascending_
			? AssetLess(lhs, rhs, sort_mode_)
			: AssetLess(rhs, lhs, sort_mode_);
	});

	std::vector<path> child_directories;
	if (!recursive && !built_in_directory) {
		const path absolute_selected{ assets_root.value() / selected_directory_ };
		if (IsDirectoryPath(absolute_selected.string())) {
			child_directories = GetChildDirectories(absolute_selected);
		}
	}
	if (show_engine_shaders_ &&
		IsBaseAssetDirectory(selected_directory_) &&
		AssetDirectoryKind(selected_directory_) == AssetKind::Shader) {
		std::erase_if(child_directories, [](const path& child) {
			return child.filename().string() == kBuiltinShaderDirectoryName;
		});
		child_directories.emplace_back(kBuiltinShaderDirectoryName);
	}

	ImGui::BeginChild(
		"##asset_grid",
		ImVec2{ 0.0f, 0.0f },
		false,
		ImGuiWindowFlags_AlwaysVerticalScrollbar
	);
	const bool child_hovered{
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup)
	};
	const int columns{ std::clamp(
		ctx.editor.GetSettings().content_browser_items_per_row,
		kMinItemsPerRow,
		kMaxItemsPerRow
	) };
	Scene* selected_scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	std::vector<AssetTileRect> tile_rects;
	bool any_tile_hovered{ false };

	if (child_directories.empty() && assets.empty()) {
		ImGui::TextDisabled("No assets or folders here.");
	}

	if (ImGui::BeginTable(
			"##asset_grid_table",
			columns,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_PadOuterX
		)) {
		for (int column{ 0 }; column < columns; ++column) {
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 1.0f);
		}

		for (const auto& child_name : child_directories) {
			const path relative{ (selected_directory_ / child_name).lexically_normal() };
			const bool virtual_builtin{ IsBuiltinShaderDirectory(relative) };
			ImGui::TableNextColumn();
			ImGui::PushID(relative.generic_string().c_str());

			const float width{ std::max(32.0f, ImGui::GetContentRegionAvail().x) };
			const float line_height{ ImGui::GetTextLineHeight() };
			const float tile_height{ width + line_height + ImGui::GetStyle().ItemSpacing.y * 2.0f };
			ImGui::InvisibleButton(
				"##folder_tile",
				ImVec2{ width, tile_height },
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
			);

			const ImVec2 item_min{ ImGui::GetItemRectMin() };
			const ImVec2 item_max{ ImGui::GetItemRectMax() };
			const bool hovered{ ImGui::IsItemHovered() };
			any_tile_hovered |= hovered;

			auto* draw_list{ ImGui::GetWindowDrawList() };
			const auto& style{ ImGui::GetStyle() };
			draw_list->AddRectFilled(
				item_min,
				item_max,
				ImGui::GetColorU32(ImGuiCol_ChildBg),
				style.ChildRounding
			);
			draw_list->AddRect(
				item_min,
				item_max,
				ImGui::GetColorU32(ImGuiCol_Border),
				style.ChildRounding
			);

			const std::string folder_text{ "Folder" };
			const ImVec2 folder_size{ ImGui::CalcTextSize(folder_text.c_str()) };
			draw_list->AddText(
				ImVec2{
					item_min.x + (width - folder_size.x) * 0.5f,
					item_min.y + std::max(4.0f, width * 0.35f)
				},
				ImGui::GetColorU32(ImGuiCol_TextDisabled),
				folder_text.c_str()
			);
			const std::string name{ child_name.string() };
			draw_list->PushClipRect(item_min, item_max, true);
			draw_list->AddText(
				ImVec2{ item_min.x + style.FramePadding.x, item_min.y + width + style.ItemSpacing.y },
				ImGui::GetColorU32(ImGuiCol_Text),
				name.c_str()
			);
			draw_list->PopClipRect();

			if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				selected_directory_ = relative;
				ClearAssetSelection();
			}

			if (!virtual_builtin) {
				ImGui::OpenPopupOnItemClick(
					"##FolderContext",
					ImGuiPopupFlags_MouseButtonRight
				);

				if (const auto move{ AcceptAssetMovePayload() };
					move.has_value() && ctx.undo.IsUndoRedoEnabled()) {
					selected_assets_ = move.value();
					MoveSelectedAssets(ctx, relative);
				}

				if (ImGui::BeginPopup("##FolderContext")) {
					if (ImGui::MenuItem("Open")) {
						selected_directory_ = relative;
						ClearAssetSelection();
					}
					ImGui::BeginDisabled(!ctx.undo.IsUndoRedoEnabled());
					if (ImGui::MenuItem("New Folder")) {
						create_folder_parent_ = relative;
						new_folder_name_.clear();
					}
					if (IsUserAssetDirectory(relative)) {
						if (ImGui::MenuItem("Rename...")) {
							rename_directory_ = relative;
							rename_directory_value_ = child_name.string();
						}
						if (ImGui::MenuItem("Delete...")) {
							pending_delete_ = PendingDeleteState{ .directory = relative };
						}
					}
					ImGui::EndDisabled();
					ImGui::EndPopup();
				}
			}

			if (hovered) {
				ImGui::SetTooltip("%s", name.c_str());
			}
			ImGui::PopID();
		}

		for (const auto& asset : assets) {
			const ContentBrowserAssetSelection identity{ asset.key, asset.kind };
			ImGui::TableNextColumn();
			ImGui::PushID(static_cast<int>(asset.kind));
			ImGui::PushID(asset.key.value.c_str());

			const auto& style{ ImGui::GetStyle() };
			const float width{ std::max(32.0f, ImGui::GetContentRegionAvail().x) };
			const float padding{ std::max(3.0f, style.FramePadding.x) };
			const float preview_size{ std::max(1.0f, width - padding * 2.0f) };
			const float tile_height{
				preview_size + ImGui::GetTextLineHeight() + padding * 3.0f
			};
			const bool selected{ !asset.engine_asset && IsAssetSelected(identity) };
			const bool default_font{
				asset.kind == AssetKind::Font && asset.key.value == kDefaultFont
			};
			const bool is_scene_asset{
				!asset.engine_asset && selected_scene && selected_scene->HasAssetDependency(asset.key)
			};
			const bool is_explicit_scene_asset{
				!asset.engine_asset && selected_scene &&
				selected_scene->HasExplicitAssetDependency(asset.key)
			};
			const bool is_resident{ asset.load_state == AssetLoadState::Loaded };

			ImGui::InvisibleButton(
				"##asset_tile",
				ImVec2{ width, tile_height },
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
			);
			const ImVec2 item_min{ ImGui::GetItemRectMin() };
			const ImVec2 item_max{ ImGui::GetItemRectMax() };
			const bool hovered{ ImGui::IsItemHovered() };
			any_tile_hovered |= hovered;

			if (!asset.engine_asset) {
				tile_rects.push_back(AssetTileRect{
					.asset = identity,
					.min = item_min,
					.max = item_max,
				});
			}

			if (!asset.engine_asset && ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				const auto& io{ ImGui::GetIO() };
				if (io.KeyCtrl || io.KeySuper) {
					ToggleSelection(identity);
				} else {
					SelectOnly(identity);
				}
			}

			if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right) &&
				!asset.engine_asset && !IsAssetSelected(identity)) {
				SelectOnly(identity);
			}

			ImGui::OpenPopupOnItemClick(
				"##AssetContext",
				ImGuiPopupFlags_MouseButtonRight
			);

			const bool can_drag_asset_key{
				asset.kind != AssetKind::Scene &&
				(!asset.engine_asset || asset.kind == AssetKind::Shader)
			};
			if (can_drag_asset_key) {
				std::span<const ContentBrowserAssetSelection> move_assets;
				if (!asset.engine_asset && IsAssetSelected(identity) && selected_assets_.size() > 1) {
					move_assets = selected_assets_;
				}
				BeginAssetKeyDragDropSource(
					asset.key.value,
					asset.kind,
					default_font
						? std::optional<std::string_view>{ "Default Font" }
						: std::optional<std::string_view>{ asset.key.value },
					move_assets,
					!asset.engine_asset
				);
			}

			if (ImGui::BeginPopup("##AssetContext")) {
				if (asset.engine_asset) {
					DrawMetadata(asset);
					if (asset.kind == AssetKind::Shader && ImGui::MenuItem("View")) {
						OpenShaderEditor(ctx, asset);
					}
				} else if (selected_assets_.size() > 1 && IsAssetSelected(identity)) {
					ImGui::TextDisabled("%zu assets selected", selected_assets_.size());
					ImGui::Separator();
					ImGui::BeginDisabled(!ctx.undo.IsUndoRedoEnabled());
					if (ImGui::MenuItem("Delete Selected...")) {
						pending_delete_ = PendingDeleteState{ .assets = selected_assets_ };
					}
					ImGui::EndDisabled();
				} else {
					DrawMetadata(asset);

					bool shader_ready{ true };
					if (asset.kind == AssetKind::Shader) {
						const auto catalog{
							assets_manager.GetCatalogAsset(asset.key, asset.kind)
						};
						shader_ready = catalog.has_value() && catalog->shader.has_value() &&
							catalog->shader->vertex.has_value() &&
							catalog->shader->fragment.has_value();
					}
					const bool can_pin{
						asset.cataloged && asset.kind != AssetKind::Scene && shader_ready &&
						!asset.key.value.empty()
					};
					const bool can_undo_asset_action{ ctx.undo.IsUndoRedoEnabled() };
					auto* assets_ptr{ &assets_manager };

					if (asset.manually_pinned) {
						ImGui::BeginDisabled(!can_undo_asset_action);
						if (ImGui::MenuItem("Remove from RAM")) {
							::ptgn::impl::AssetAccessor{ assets_manager }.Unload(asset.key, asset.kind);
							ctx.undo.PushApplied(
								"Remove Asset RAM Pin",
								[assets_ptr, key = asset.key, kind = asset.kind]() {
									assets_ptr->LoadAssetAsync(key, kind);
								},
								[assets_ptr, key = asset.key, kind = asset.kind]() {
									::ptgn::impl::AssetAccessor{ *assets_ptr }.Unload(key, kind);
								},
								false
							);
						}
						ImGui::EndDisabled();
					} else {
						ImGui::BeginDisabled(!can_pin || !can_undo_asset_action);
						if (ImGui::MenuItem("Add to RAM")) {
							assets_manager.LoadAssetAsync(asset.key, asset.kind);
							ctx.undo.PushApplied(
								"Add Asset RAM Pin",
								[assets_ptr, key = asset.key, kind = asset.kind]() {
									::ptgn::impl::AssetAccessor{ *assets_ptr }.Unload(key, kind);
								},
								[assets_ptr, key = asset.key, kind = asset.kind]() {
									assets_ptr->LoadAssetAsync(key, kind);
								},
								false
							);
						}
						ImGui::EndDisabled();
					}

					if (asset.globally_pinned) {
						ImGui::BeginDisabled(!can_undo_asset_action);
						if (ImGui::MenuItem("Remove from Project Preload")) {
							assets_manager.RemoveProjectAssetDependency(asset.key);
							ctx.undo.PushApplied(
								"Remove Project Preload Asset",
								[assets_ptr, key = asset.key]() { assets_ptr->PreloadProjectAsset(key); },
								[assets_ptr, key = asset.key]() { assets_ptr->RemoveProjectAssetDependency(key); }
							);
						}
						ImGui::EndDisabled();
					} else {
						ImGui::BeginDisabled(!can_pin || !can_undo_asset_action);
						if (ImGui::MenuItem("Add to Project Preload")) {
							assets_manager.PreloadProjectAsset(asset.key);
							ctx.undo.PushApplied(
								"Add Project Preload Asset",
								[assets_ptr, key = asset.key]() { assets_ptr->RemoveProjectAssetDependency(key); },
								[assets_ptr, key = asset.key]() { assets_ptr->PreloadProjectAsset(key); }
							);
						}
						ImGui::EndDisabled();
					}

					if (selected_scene && asset.kind != AssetKind::Scene) {
						const bool scene_editable{
							!selected_scene->IsRuntime() && asset.cataloged &&
							ctx.undo.IsUndoRedoEnabled()
						};
						const std::string scene_key{ selected_scene->GetTag() };
						auto* editor_ptr{ &ctx.editor };
						auto apply_dependency = [editor_ptr, scene_key, key = asset.key](bool add) {
							auto& manager{ editor_ptr->GetSceneManager() };
							const auto scene_hash{ Hash(scene_key) };
							if (!manager.HasScene(scene_hash)) {
								return;
							}
							auto& scene{ manager.GetScene(scene_hash) };
							if (add) {
								scene.AddAssetDependency(key);
							} else {
								scene.RemoveAssetDependency(key);
							}
							scene.SyncAssetDependenciesFromSerialization();
						};

						if (is_explicit_scene_asset) {
							ImGui::BeginDisabled(!scene_editable);
							if (ImGui::MenuItem("Remove from Selected Scene")) {
								apply_dependency(false);
								ctx.undo.PushApplied(
									"Remove Scene Asset Dependency",
									[apply_dependency]() mutable { apply_dependency(true); },
									[apply_dependency]() mutable { apply_dependency(false); }
								);
							}
							ImGui::EndDisabled();
						} else if (is_scene_asset) {
							ImGui::BeginDisabled();
							ImGui::MenuItem("Referenced by Selected Scene");
							ImGui::EndDisabled();
						} else {
							ImGui::BeginDisabled(!scene_editable);
							if (ImGui::MenuItem("Add to Selected Scene")) {
								apply_dependency(true);
								ctx.undo.PushApplied(
									"Add Scene Asset Dependency",
									[apply_dependency]() mutable { apply_dependency(false); },
									[apply_dependency]() mutable { apply_dependency(true); }
								);
							}
							ImGui::EndDisabled();
						}
					}

					if (asset.kind == AssetKind::Shader) {
						ImGui::Separator();
						if (ImGui::MenuItem("Edit")) {
							OpenShaderEditor(ctx, asset);
						}
					}

					ImGui::Separator();
					ImGui::BeginDisabled(
						!ctx.undo.IsUndoRedoEnabled() || asset.kind == AssetKind::Scene ||
						default_font || AssetReferencedByEditor(ctx, asset.key)
					);
					if (ImGui::MenuItem("Rename Key...")) {
						rename_asset_key_ = identity;
						rename_asset_key_value_ = asset.key.value;
					}
					ImGui::EndDisabled();

					ImGui::BeginDisabled(
						!ctx.undo.IsUndoRedoEnabled() || !IsDeletingAssetAllowed(asset)
					);
					if (ImGui::MenuItem("Delete from Project...")) {
						pending_delete_ = PendingDeleteState{ .assets = { identity } };
					}
					ImGui::EndDisabled();
				}
				ImGui::EndPopup();
			}

			auto* draw_list{ ImGui::GetWindowDrawList() };
			ImU32 tile_background{ ImGui::GetColorU32(ImGuiCol_ChildBg) };
			if (asset.globally_pinned) {
				tile_background = ImGui::GetColorU32(kProjectAssetBackground);
			} else if (is_scene_asset) {
				tile_background = ImGui::GetColorU32(kSceneAssetBackground);
			}
			draw_list->AddRectFilled(
				item_min,
				item_max,
				tile_background,
				style.ChildRounding
			);
			if (selected) {
				draw_list->AddRect(
					item_min,
					item_max,
					ImGui::GetColorU32(kSelectedAssetBorder),
					style.ChildRounding,
					0,
					3.0f
				);
			}

			ImU32 preview_border{ ImGui::GetColorU32(ImGuiCol_Border) };
			if (is_resident) {
				preview_border = ImGui::GetColorU32(kResidentAssetBorder);
			}
			if (asset.compile_error) {
				preview_border = ImGui::GetColorU32(kShaderErrorBorder);
			}
			DrawPreviewAt(
				ImVec2{ item_min.x + padding, item_min.y + padding },
				preview_size,
				GetTilePreview(
					asset,
					static_cast<::ptgn::impl::TextureId>(audio_icon_texture_),
					static_cast<::ptgn::impl::TextureId>(document_icon_texture_)
				),
				preview_border
			);

			const std::string label{
				default_font
					? "Default Font"
					: asset.engine_asset
						? asset.source_path.stem().string()
						: asset.key.value
			};
			const ImVec2 text_min{
				item_min.x + padding,
				item_min.y + padding + preview_size + padding
			};
			draw_list->PushClipRect(
				text_min,
				ImVec2{ item_max.x - padding, item_max.y },
				true
			);
			draw_list->AddText(text_min, ImGui::GetColorU32(ImGuiCol_Text), label.c_str());
			draw_list->PopClipRect();

			if (hovered) {
				ImGui::SetTooltip("%s", label.c_str());
			}

			ImGui::PopID();
			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	const auto& io{ ImGui::GetIO() };
	if (child_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !any_tile_hovered) {
		selection_box_ = SelectionBoxState{
			.start_x = io.MousePos.x,
			.start_y = io.MousePos.y,
			.base_selection =
				(io.KeyCtrl || io.KeySuper)
					? selected_assets_
					: std::vector<ContentBrowserAssetSelection>{},
		};
		if (!(io.KeyCtrl || io.KeySuper)) {
			ClearAssetSelection();
		}
	}

	if (selection_box_.has_value()) {
		const ImVec2 a{ selection_box_->start_x, selection_box_->start_y };
		const ImVec2 b{ io.MousePos.x, io.MousePos.y };
		const ImVec2 selection_min{ std::min(a.x, b.x), std::min(a.y, b.y) };
		const ImVec2 selection_max{ std::max(a.x, b.x), std::max(a.y, b.y) };
		selected_assets_ = selection_box_->base_selection;
		for (const auto& tile : tile_rects) {
			if (RectsOverlap(selection_min, selection_max, tile.min, tile.max) &&
				!std::ranges::contains(selected_assets_, tile.asset)) {
				selected_assets_.emplace_back(tile.asset);
			}
		}
		ImGui::GetForegroundDrawList()->AddRect(
			selection_min,
			selection_max,
			ImGui::GetColorU32(ImGuiCol_DragDropTarget),
			0.0f,
			0,
			1.5f
		);
		ImGui::GetForegroundDrawList()->AddRectFilled(
			selection_min,
			selection_max,
			ImGui::GetColorU32(ImVec4{ 1.0f, 1.0f, 1.0f, 0.04f })
		);
		if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			selection_box_.reset();
		}
	}

	ImGui::EndChild();
}

void ContentBrowserPanel::DrawContentBrowserPopups(EditorContext& ctx) {
	if (create_folder_parent_.has_value()) {
		ImGui::OpenPopup("Create Asset Folder###ContentBrowserCreateFolder");
		if (ImGui::BeginPopupModal(
				"Create Asset Folder###ContentBrowserCreateFolder",
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize
			)) {
			if (ImGui::IsWindowAppearing()) {
				ImGui::SetKeyboardFocusHere();
			}
			bool submit{ ImGui::InputText(
				"Name",
				&new_folder_name_,
				ImGuiInputTextFlags_EnterReturnsTrue
			) };
			const bool valid{ !new_folder_name_.empty() && new_folder_name_ != "." &&
				new_folder_name_ != ".." && new_folder_name_.find_first_of("/\\") == std::string::npos };
			ImGui::BeginDisabled(!valid);
			submit |= ImGui::Button("Create");
			if (submit && valid) {
				if (CreateDirectory(ctx, create_folder_parent_.value(), new_folder_name_)) {
					create_folder_parent_.reset();
					new_folder_name_.clear();
					ImGui::CloseCurrentPopup();
				} else {
					status_ = "Could not create the asset directory.";
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				create_folder_parent_.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	if (rename_directory_.has_value()) {
		ImGui::OpenPopup("Rename Asset Directory###ContentBrowserRenameDirectory");
		if (ImGui::BeginPopupModal(
				"Rename Asset Directory###ContentBrowserRenameDirectory",
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize
			)) {
			ImGui::InputText("Name", &rename_directory_value_);
			const bool valid{ !rename_directory_value_.empty() &&
				rename_directory_value_.find_first_of("/\\") == std::string::npos };
			ImGui::BeginDisabled(!valid);
			if (ImGui::Button("Rename")) {
				if (RenameDirectory(ctx, rename_directory_.value(), rename_directory_value_)) {
					rename_directory_.reset();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				rename_directory_.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	if (rename_asset_key_.has_value()) {
		ImGui::OpenPopup("Rename Asset Key###ContentBrowserRenameAssetKey");
		if (ImGui::BeginPopupModal(
				"Rename Asset Key###ContentBrowserRenameAssetKey",
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize
			)) {
			ImGui::InputText("Key", &rename_asset_key_value_);
			const bool unchanged{
				rename_asset_key_value_ == rename_asset_key_->key.value
			};
			const bool valid{
				!rename_asset_key_value_.empty() &&
				(unchanged || !ctx.editor.GetAssetManager().HasCatalogAsset(
					AssetKey{ rename_asset_key_value_ },
					rename_asset_key_->kind
				))
			};
			ImGui::BeginDisabled(!valid);
			if (ImGui::Button("Rename")) {
				if (RenameAssetKey(ctx, rename_asset_key_.value(), rename_asset_key_value_)) {
					rename_asset_key_.reset();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::EndDisabled();
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				rename_asset_key_.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	if (pending_delete_.has_value()) {
		auto& pending{ pending_delete_.value() };
		if (pending.directory.has_value() && pending.listed_files.empty()) {
			const auto root{ ctx.editor.GetAssetManager().GetAssetDirectory() };
			if (root.has_value()) {
				const path absolute{ root.value() / pending.directory.value() };
				std::error_code error;
				for (std::filesystem::recursive_directory_iterator it{ absolute, error }, end;
					 !error && it != end; it.increment(error)) {
					std::error_code entry_error;
					if (it->is_regular_file(entry_error) && !entry_error) {
						pending.listed_files.emplace_back(
							it->path().lexically_relative(absolute).lexically_normal()
						);
					}
				}
				std::ranges::sort(pending.listed_files);
			}
		}
		ImGui::OpenPopup("Confirm Content Browser Delete###ContentBrowserDeleteConfirm");
		ImGui::SetNextWindowSizeConstraints(ImVec2{ 480.0f, 0.0f }, ImVec2{ 760.0f, 620.0f });
		if (ImGui::BeginPopupModal(
				"Confirm Content Browser Delete###ContentBrowserDeleteConfirm",
				nullptr,
				ImGuiWindowFlags_AlwaysAutoResize
			)) {
			if (pending.directory.has_value()) {
				ImGui::TextWrapped(
					"Are you sure you want to delete Assets/%s and all its files?",
					pending.directory->generic_string().c_str()
				);
				if (pending.listed_files.empty()) {
					ImGui::TextDisabled("The directory is empty.");
				} else {
					ImGui::SeparatorText("Files that will be deleted");
					ImGui::BeginChild("##delete_file_list", ImVec2{ 0.0f, std::min(260.0f, ImGui::GetTextLineHeightWithSpacing() * static_cast<float>(pending.listed_files.size() + 1)) }, true);
					for (const auto& file : pending.listed_files) {
						ImGui::TextUnformatted(file.generic_string().c_str());
					}
					ImGui::EndChild();
				}
			} else {
				ImGui::TextWrapped(
					"Are you sure you want to delete %zu asset%s and their files?",
					pending.assets.size(),
					pending.assets.size() == 1 ? "" : "s"
				);
				ImGui::SeparatorText("Assets that will be deleted");
				for (const auto& selected : pending.assets) {
					const auto asset{ ctx.editor.GetAssetManager().GetCatalogAsset(
						selected.key,
						selected.kind
					) };
					if (asset.has_value()) {
						ImGui::Text(
							"%s  [%s]",
							selected.key.value.c_str(),
							asset->source_path.generic_string().c_str()
						);
					} else {
						ImGui::TextUnformatted(selected.key.value.c_str());
					}
				}
			}
			ImGui::Separator();
			if (ImGui::Button("Delete")) {
				const bool deleted{
					pending.directory.has_value()
						? DeleteDirectory(ctx, pending.directory.value())
						: DeleteAssets(ctx, pending.assets)
				};
				if (deleted) {
					pending_delete_.reset();
					ImGui::CloseCurrentPopup();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Cancel")) {
				pending_delete_.reset();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}
void ContentBrowserPanel::OpenShaderEditor(
	EditorContext& ctx,
	const ::ptgn::impl::AssetRecord& asset
) {
	const auto is_dirty = [](const ShaderEditorState& value) {
		return value.vertex_source != value.saved_vertex_source ||
			value.fragment_source != value.saved_fragment_source ||
			!ShaderProgramsEqual(value.program, value.saved_program);
	};

	if (shader_editor_.has_value() && shader_editor_->key != ShaderKey{ asset.key } &&
		!shader_editor_->read_only && is_dirty(shader_editor_.value())) {
		shader_editor_->open = true;
		status_ = "Save, discard, or close the current shader before opening another shader.";
		return;
	}
	if (shader_editor_.has_value() && shader_editor_->key == ShaderKey{ asset.key }) {
		shader_editor_->open = true;
		return;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	auto source{ assets.GetShaderSource(ShaderKey{ asset.key }) };
	if (!source.has_value()) {
		status_ = "Could not read shader source for " + asset.key.value;
		return;
	}

	const ShaderStageMask stages{ DetectShaderStages(source.value()) };
	SerializedShaderProgram program;
	if (!asset.engine_asset) {
		if (const auto catalog{ assets.GetCatalogAsset(asset.key, AssetKind::Shader) };
			catalog.has_value() && catalog->shader.has_value()) {
			program = catalog->shader.value();
		}
		program = ResolveShaderProgramForSource(assets, source.value(), program);
	} else if (const auto suggested{ assets.SuggestShaderProgram(source.value()) }) {
		program = suggested.value();
	}

	const auto resolve_stage = [&](ShaderStageMask stage, const std::optional<std::string>& reference) {
		if (HasShaderStage(stages, stage)) {
			return StripShaderStageDirective(
				::ptgn::impl::ExtractShaderStageSource(source.value(), stage)
			);
		}
		if (!reference.has_value() || reference->empty()) {
			return std::string{};
		}
		return StripShaderStageDirective(
			assets.ResolveShaderStageSource(source.value(), reference.value(), stage)
				.value_or(std::string{})
		);
	};

	const bool read_only{ asset.read_only || asset.engine_asset };
	const std::string vertex_source{ resolve_stage(ShaderStageMask::Vertex, program.vertex) };
	const std::string fragment_source{ resolve_stage(ShaderStageMask::Fragment, program.fragment) };
	shader_editor_ = ShaderEditorState{
		.key = ShaderKey{ asset.key },
		.display_name = asset.engine_asset ? asset.source_path.stem().string() : asset.key.value,
		.vertex_source = vertex_source,
		.fragment_source = fragment_source,
		.saved_vertex_source = vertex_source,
		.saved_fragment_source = fragment_source,
		.program = program,
		.saved_program = program,
		.source_stages = stages,
		.diagnostics = asset.compile_log,
		.last_compile_success = !asset.compile_error,
		.vertex_editable = !read_only && HasShaderStage(stages, ShaderStageMask::Vertex),
		.fragment_editable = !read_only && HasShaderStage(stages, ShaderStageMask::Fragment),
		.read_only = read_only,
		.open = true,
	};
}

void ContentBrowserPanel::OpenShaderEditor(EditorContext& ctx, const ShaderKey& key) {
	auto& assets{ ctx.editor.GetAssetManager() };
	auto records{ ::ptgn::impl::AssetAccessor{ assets }.GetAssets() };
	auto engine_records{ assets.GetEngineShaderAssets() };
	std::ranges::move(engine_records, std::back_inserter(records));

	const auto it{ std::ranges::find_if(records, [&](const auto& record) {
		return record.kind == AssetKind::Shader &&
			record.key == static_cast<const AssetKey&>(key);
	}) };
	if (it == records.end()) {
		status_ = "Could not find shader " + key.value;
		return;
	}
	OpenShaderEditor(ctx, *it);
}

bool ContentBrowserPanel::CanApplicationClose() {
	if (!shader_editor_.has_value() || shader_editor_->read_only) {
		return true;
	}
	const auto& editor{ shader_editor_.value() };
	if (editor.vertex_source == editor.saved_vertex_source &&
		editor.fragment_source == editor.saved_fragment_source &&
		ShaderProgramsEqual(editor.program, editor.saved_program)) {
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

	const auto build_file_source = [&](bool saved) {
		std::string output;
		auto append_stage = [&](std::string_view type, const std::string& source) {
			if (!output.empty() && output.back() != '\n') {
				output.push_back('\n');
			}
			output += "#type ";
			output += type;
			output.push_back('\n');
			output += StripShaderStageDirective(source);
			if (output.empty() || output.back() != '\n') {
				output.push_back('\n');
			}
		};

		if (HasShaderStage(editor.source_stages, ShaderStageMask::Vertex)) {
			append_stage("vertex", saved ? editor.saved_vertex_source : editor.vertex_source);
		}
		if (HasShaderStage(editor.source_stages, ShaderStageMask::Fragment)) {
			append_stage("fragment", saved ? editor.saved_fragment_source : editor.fragment_source);
		}
		return output;
	};

	const auto is_dirty = [&]() {
		return editor.vertex_source != editor.saved_vertex_source ||
			editor.fragment_source != editor.saved_fragment_source ||
			!ShaderProgramsEqual(editor.program, editor.saved_program);
	};

	bool open{ editor.open };
	std::string title{
		(editor.read_only ? "Shader Viewer - " : "Shader Editor - ") + editor.display_name
	};
	if (!editor.read_only && is_dirty()) {
		title += " *";
	}
	title += "###ShaderEditor";

	const auto* viewport{ ImGui::GetMainViewport() };
	const ImVec2 max_size{
		std::max(320.0f, viewport->WorkSize.x - 40.0f),
		std::max(260.0f, viewport->WorkSize.y - 40.0f),
	};
	const ImVec2 desired_size{
		std::min(max_size.x, std::max(720.0f, viewport->WorkSize.x * 0.82f)),
		std::min(max_size.y, std::max(560.0f, viewport->WorkSize.y * 0.82f)),
	};
	ImGui::SetNextWindowPos(
		ImVec2{
			viewport->WorkPos.x + viewport->WorkSize.x * 0.5f,
			viewport->WorkPos.y + viewport->WorkSize.y * 0.5f,
		},
		ImGuiCond_Appearing,
		ImVec2{ 0.5f, 0.5f }
	);
	ImGui::SetNextWindowSize(desired_size, ImGuiCond_Appearing);

	const ImGuiWindowFlags window_flags{
		ImGuiWindowFlags_NoScrollbar |
		ImGuiWindowFlags_NoScrollWithMouse |
		(editor.read_only ? ImGuiWindowFlags_None : ImGuiWindowFlags_MenuBar)
	};
	if (ImGui::Begin(title.c_str(), &open, window_flags)) {
		if (!editor.read_only && ImGui::BeginMenuBar()) {
			if (ImGui::MenuItem("Save", "Ctrl+S")) {
				pending_shader_save_action_ = PendingShaderSaveAction::SaveOnly;
			}
			if (ImGui::MenuItem("Save + Recompile")) {
				pending_shader_save_action_ = PendingShaderSaveAction::SaveAndRecompile;
			}
			if (ImGui::MenuItem("Recompile Unsaved")) {
				const std::string source{ build_file_source(false) };
				auto result{ assets.RecompileShaderSource(editor.key, source, editor.program) };
				editor.last_compile_success = result.success;
				editor.diagnostics = std::move(result.log);
			}
			ImGui::EndMenuBar();
		}

		auto records{ ::ptgn::impl::AssetAccessor{ assets }.GetAssets() };
		auto owner_it{ std::ranges::find_if(records, [&](const auto& record) {
			return record.kind == AssetKind::Shader &&
				record.key == static_cast<const AssetKey&>(editor.key);
		}) };
		::ptgn::impl::AssetRecord owner;
		if (owner_it != records.end()) {
			owner = *owner_it;
		} else {
			auto engine_records{ assets.GetEngineShaderAssets() };
			const auto engine_it{ std::ranges::find_if(engine_records, [&](const auto& record) {
				return record.key == static_cast<const AssetKey&>(editor.key);
			}) };
			if (engine_it != engine_records.end()) {
				owner = *engine_it;
			}
		}
		owner.metadata.shader_stages = editor.source_stages;

		const char* diagnostic_text{
			editor.diagnostics.empty() ? "No compiler messages." : editor.diagnostics.c_str()
		};
		float diagnostics_height{ 0.0f };
		float diagnostics_header_height{ 0.0f };
		if (!editor.read_only) {
			const float output_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
			const float output_text_height{
				ImGui::CalcTextSize(diagnostic_text, nullptr, false, output_width).y
			};
			diagnostics_height = std::min(
				180.0f,
				std::max(
					ImGui::GetTextLineHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f,
					output_text_height + ImGui::GetStyle().WindowPadding.y * 2.0f
				)
			);
			diagnostics_header_height =
				ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
		}

		const float stage_area_height{
			editor.read_only
				? std::max(100.0f, ImGui::GetContentRegionAvail().y)
				: std::max(
					180.0f,
					ImGui::GetContentRegionAvail().y - diagnostics_height - diagnostics_header_height
				)
		};
		ImGui::BeginChild(
			"##ShaderStages",
			ImVec2{ -FLT_MIN, stage_area_height },
			false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse
		);
		if (ImGui::BeginTabBar("##ShaderStageTabs")) {
			auto draw_stage_tab = [&](const char* tab_name, ShaderStageMask stage) {
				if (!ImGui::BeginTabItem(tab_name)) {
					return;
				}

				const bool vertex{ stage == ShaderStageMask::Vertex };
				bool& editable{ vertex ? editor.vertex_editable : editor.fragment_editable };
				std::string& source{ vertex ? editor.vertex_source : editor.fragment_source };
				auto& reference{ vertex ? editor.program.vertex : editor.program.fragment };

				if (!editable && !editor.read_only) {
					std::string selected{ reference.value_or(std::string{}) };
					if (DrawShaderSourceCombo("Source", selected, stage, owner, assets)) {
						reference = selected.empty()
							? std::nullopt
							: std::optional<std::string>{ selected };
						const std::string owner_source{ build_file_source(false) };
						source = reference.has_value()
							? StripShaderStageDirective(
								assets.ResolveShaderStageSource(
									owner_source,
									reference.value(),
									stage
								).value_or(std::string{})
							)
							: std::string{};
					}
				}

				ImGuiInputTextFlags flags{ ImGuiInputTextFlags_AllowTabInput };
				if (!editable) {
					flags |= ImGuiInputTextFlags_ReadOnly;
				}
				ImGui::InputTextMultiline(
					vertex ? "##VertexShaderSource" : "##FragmentShaderSource",
					&source,
					ImVec2{ -FLT_MIN, -FLT_MIN },
					flags
				);
				ImGui::EndTabItem();
			};

			draw_stage_tab("Vertex", ShaderStageMask::Vertex);
			draw_stage_tab("Fragment", ShaderStageMask::Fragment);
			ImGui::EndTabBar();
		}
		ImGui::EndChild();

		if (!editor.read_only) {
			ImGui::SeparatorText("Compiler Output");
			if (!editor.last_compile_success) {
				ImGui::PushStyleColor(ImGuiCol_Text, kShaderErrorBorder);
			}
			ImGui::BeginChild(
				"##ShaderDiagnostics",
				ImVec2{ -FLT_MIN, diagnostics_height },
				true
			);
			ImGui::TextWrapped("%s", diagnostic_text);
			ImGui::EndChild();
			if (!editor.last_compile_success) {
				ImGui::PopStyleColor();
			}
		}
	}
	ImGui::End();

	if (!open && editor.open) {
		if (!editor.read_only && is_dirty()) {
			pending_shader_close_confirmation_ = true;
		} else {
			editor.open = false;
		}
	}

	auto perform_save = [&](PendingShaderSaveAction action, bool allow_invalid) {
		const std::string source{ build_file_source(false) };
		auto validation{ assets.ValidateShaderSource(editor.key, source, editor.program) };
		editor.last_compile_success = validation.success;
		editor.diagnostics = validation.log;
		if (!validation.success && !allow_invalid) {
			pending_shader_save_action_ = action;
			pending_shader_compile_error_confirmation_ = true;
			return;
		}

		const std::string before_source{ build_file_source(true) };
		const SerializedShaderProgram before_program{ editor.saved_program };
		const std::string after_source{ source };
		const SerializedShaderProgram after_program{ editor.program };
		const bool recompile_runtime{
			validation.success && action == PendingShaderSaveAction::SaveAndRecompile
		};

		if (!assets.SaveShaderSource(editor.key, after_source, validation)) {
			editor.last_compile_success = false;
			editor.diagnostics += "\nCould not write shader file to disk.";
			return;
		}
		if (!assets.ConfigureShaderProgram(
			editor.key,
			after_program.vertex,
			after_program.fragment
		)) {
			editor.last_compile_success = false;
			editor.diagnostics += "\nCould not save shader program configuration.";
			return;
		}

		editor.saved_vertex_source = editor.vertex_source;
		editor.saved_fragment_source = editor.fragment_source;
		editor.saved_program = after_program;

		if (before_source != after_source ||
			!ShaderProgramsEqual(before_program, after_program)) {
			auto* assets_ptr{ &assets };
			const ShaderKey key{ editor.key };
			auto apply_shader_state = [assets_ptr, key, recompile_runtime](
				const std::string& state_source,
				const SerializedShaderProgram& program
			) {
				auto state_validation{
					assets_ptr->ValidateShaderSource(key, state_source, program)
				};
				if (!assets_ptr->SaveShaderSource(key, state_source, state_validation)) {
					return;
				}
				if (!assets_ptr->ConfigureShaderProgram(key, program.vertex, program.fragment)) {
					return;
				}
				if (recompile_runtime && state_validation.success) {
					static_cast<void>(
					assets_ptr->RecompileShaderSource(key, state_source, program)
				);
				}
			};

			ctx.undo.PushApplied(
				"Save Shader",
				[apply_shader_state, before_source, before_program]() mutable {
					apply_shader_state(before_source, before_program);
				},
				[apply_shader_state, after_source, after_program]() mutable {
					apply_shader_state(after_source, after_program);
				}
			);
		}

		if (recompile_runtime) {
			auto result{ assets.RecompileShaderSource(editor.key, after_source, after_program) };
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
	if (ImGui::BeginPopupModal(
		"Save Shader With Compile Errors?",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	)) {
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
	if (ImGui::BeginPopupModal(
		"Unsaved Shader Changes",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	)) {
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
		"Unsaved Shader Changes Before Exit",
		nullptr,
		ImGuiWindowFlags_AlwaysAutoResize
	)) {
		ImGui::TextWrapped(
			"The shader editor has unsaved changes. Save before exiting the application?"
		);
		if (ImGui::Button("Save and Exit")) {
			application_close_requested_ = false;
			pending_shader_save_action_ = PendingShaderSaveAction::SaveAndExit;
			ImGui::CloseCurrentPopup();
		}
		ImGui::SameLine();
		if (ImGui::Button("Discard and Exit")) {
			editor.saved_vertex_source = editor.vertex_source;
			editor.saved_fragment_source = editor.fragment_source;
			editor.saved_program = editor.program;
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
