#include "panels/content_browser.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cfloat>
#include <cstring>
#include <magic_enum/magic_enum.hpp>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/graphics/surface.h"
#include "core/util/string.h"
#include "panels/content_browser_icons.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "renderer/resources/texture_format.h"
#include "runtime/asset/asset_key.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/graphics/text/font_system.h"
#include "tools/debug/debug_system.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

inline constexpr int kMaxItemsPerRow{ 10 };
inline constexpr char kAssetKeyPayloadType[]{ "PTGN_ASSET_KEY" };
inline constexpr V2_int kEmbeddedIconSize{ 64, 64 };

struct AssetKeyPayload {
	AssetKind kind{ AssetKind::Unknown };
	char key[256]{};
};

struct TilePreview {
	impl::TextureId texture;
	V2_int size;
	bool tint_with_text_color{ false };
};

inline void BeginAssetKeyDragDropSource(
	std::string_view key, AssetKind kind, std::optional<std::string_view> key_alias = std::nullopt
) {
	if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		return;
	}

	AssetKeyPayload payload;
	payload.kind = kind;

	auto length{ std::min(key.size(), sizeof(payload.key) - 1) };
	std::memcpy(payload.key, key.data(), length);
	payload.key[length] = '\0';

	ImGui::SetDragDropPayload(kAssetKeyPayloadType, &payload, sizeof(payload));

	auto text{ key_alias.value_or(key) };
	ImGui::TextUnformatted(text.data(), text.data() + text.size());

	ImGui::EndDragDropSource();
}

bool ContainsInsensitive(std::string_view value, std::string_view query) {
	auto lower_value{ ToLower(std::string{ value }) };
	auto lower_query{ ToLower(std::string{ query }) };

	return lower_value.find(lower_query) != std::string::npos;
}

std::string SanitizeAssetKey(std::string key) {
	if (key.empty()) {
		return "asset";
	}

	for (auto& c : key) {
		auto valid{ std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' };
		if (!valid) {
			c = '_';
		}
	}

	return key;
}

AssetKey DefaultAssetKey(const path& asset_path) {
	auto key{ asset_path.stem().string() };
	return SanitizeAssetKey(key);
}

AssetKey MakeUniqueAssetKey(AssetManager& assets, const path& asset_path) {
	auto base_key{ DefaultAssetKey(asset_path) };
	auto key{ base_key };

	for (auto suffix{ 1 }; assets.Has(key); ++suffix) {
		key = base_key.value + "_" + std::to_string(suffix);
	}

	return key;
}

FileDialog::Options ImportOptions() {
	return FileDialog::Options{
		.filters = {
			FileDialog::Filter{ .name = "Images", .spec = "png,jpg,jpeg,bmp,gif" },
			FileDialog::Filter{ .name = "Audio", .spec = "ogg,mp3,wav,opus" },
			FileDialog::Filter{ .name = "Fonts", .spec = "ttf,otf" },
			FileDialog::Filter{ .name = "Shaders", .spec = "glsl" },
			FileDialog::Filter{ .name = "Json", .spec = "json" },
			FileDialog::Filter{ .name = "All files", .spec = "*" },
		},
	};
}

void ImportAssetPaths(EditorContext& ctx, std::span<const path> asset_paths, std::string& status) {
	auto imported_count{ 0 };
	auto skipped_count{ 0 };

	for (auto& asset_path : asset_paths) {
		auto kind{ impl::GetAssetKind(asset_path) };

		if (kind == AssetKind::Unknown) {
			++skipped_count;
			continue;
		}

		auto key{ MakeUniqueAssetKey(ctx.editor.GetAssetManager(), asset_path) };
		ctx.editor.GetAssetManager().Load(key, asset_path);

		++imported_count;
	}

	status = "Imported " + std::to_string(imported_count) + " asset";
	if (imported_count != 1) {
		status += "s";
	}

	if (skipped_count > 0) {
		status += ", skipped " + std::to_string(skipped_count) + " unsupported file";
		if (skipped_count != 1) {
			status += "s";
		}
	}
}

bool MatchesSearch(const impl::AssetRecord& asset, std::string_view search) {
	if (search.empty()) {
		return true;
	}

	if (ContainsInsensitive(asset.key.value, search)) {
		return true;
	}

	if (ContainsInsensitive(magic_enum::enum_name(asset.kind), search)) {
		return true;
	}

	if (!asset.source_path.empty() &&
		ContainsInsensitive(asset.source_path.filename().string(), search)) {
		return true;
	}

	return false;
}

impl::TextureObject CreateEmbeddedIconTexture(
	Renderer& renderer, std::span<const std::uint8_t> png
) {
	impl::Surface surface{ png, 4 };

	return impl::RendererAccessor{ renderer }.CreateTexture(
		surface.Data(), TextureDesc{
							.size{ surface.GetSize() },
							.format = TextureFormat::RGBA8,
							.params{ TextureMinFilter::Linear, TextureMagFilter::Linear },
						}
	);
}

std::optional<TilePreview> GetTilePreview(
	const impl::AssetRecord& asset, impl::TextureId audio_icon, impl::TextureId document_icon
) {
	if (asset.preview.has_value()) {
		return TilePreview{
			.texture = asset.preview->texture,
			.size	 = asset.preview->size,
		};
	}

	switch (asset.kind) {
		case AssetKind::Audio:
			return TilePreview{
				.texture			  = audio_icon,
				.size				  = kEmbeddedIconSize,
				.tint_with_text_color = true,
			};

		case AssetKind::Shader: [[fallthrough]];
		case AssetKind::Json:
			return TilePreview{
				.texture			  = document_icon,
				.size				  = kEmbeddedIconSize,
				.tint_with_text_color = true,
			};

		case AssetKind::Texture: [[fallthrough]];
		case AssetKind::Font:	 [[fallthrough]];
		case AssetKind::Unknown: break;
	}

	return std::nullopt;
}

void DrawWrappedText(std::string_view text, float width, bool disabled = false) {
	auto wrap_x{ ImGui::GetCursorPosX() + std::max(1.0f, width) };

	ImGui::PushTextWrapPos(wrap_x);

	if (disabled) {
		ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
	}

	ImGui::TextUnformatted(text.data(), text.data() + text.size());

	if (disabled) {
		ImGui::PopStyleColor();
	}

	ImGui::PopTextWrapPos();

	if (ImGui::IsItemHovered() &&
		ImGui::CalcTextSize(text.data(), text.data() + text.size()).x > width) {
		ImGui::SetTooltip("%.*s", static_cast<int>(text.size()), text.data());
	}
}

void DrawPreview(float preview_size, const std::optional<TilePreview>& preview) {
	auto preview_min{ ImGui::GetCursorScreenPos() };

	ImGui::InvisibleButton("##preview", ImVec2{ preview_size, preview_size });

	auto preview_max{ ImVec2{ preview_min.x + preview_size, preview_min.y + preview_size } };
	auto* draw_list{ ImGui::GetWindowDrawList() };
	auto& style{ ImGui::GetStyle() };

	draw_list->AddRectFilled(
		preview_min, preview_max, ImGui::GetColorU32(ImGuiCol_FrameBg), style.FrameRounding
	);
	draw_list->AddRect(
		preview_min, preview_max, ImGui::GetColorU32(ImGuiCol_Border), style.FrameRounding
	);

	if (!preview.has_value() || !preview->texture || !preview->size.IsPositive()) {
		return;
	}

	constexpr float kPreviewPadding{ 6.0f };

	auto content_size{ std::max(1.0f, preview_size - kPreviewPadding * 2.0f) };
	auto source_width{ static_cast<float>(preview->size.x) };
	auto source_height{ static_cast<float>(preview->size.y) };
	auto scale{ std::min(content_size / source_width, content_size / source_height) };

	ImVec2 image_size{ source_width * scale, source_height * scale };
	ImVec2 image_min{
		preview_min.x + (preview_size - image_size.x) * 0.5f,
		preview_min.y + (preview_size - image_size.y) * 0.5f,
	};
	ImVec2 image_max{ image_min.x + image_size.x, image_min.y + image_size.y };

	auto tint{ preview->tint_with_text_color ? ImGui::GetStyleColorVec4(ImGuiCol_Text)
											 : ImVec4{ 1.0f, 1.0f, 1.0f, 1.0f } };

	draw_list->AddImage(
		static_cast<ImTextureID>(preview->texture), image_min, image_max, ImVec2{ 0.0f, 0.0f },
		ImVec2{ 1.0f, 1.0f }, ImGui::GetColorU32(tint)
	);
}

bool DrawSortDirectionButton(
	const char* id, ImGuiDir direction, bool selected, const char* tooltip
) {
	if (selected) {
		auto active{ ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive) };
		ImGui::PushStyleColor(ImGuiCol_Button, active);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, active);
	}

	bool pressed{ ImGui::ArrowButton(id, direction) };

	if (selected) {
		ImGui::PopStyleColor(2);
	}

	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}

	return pressed;
}

bool AssetLess(
	const impl::AssetRecord& a, const impl::AssetRecord& b, ContentBrowserPanel::SortMode sort_mode
) {
	auto a_kind{ magic_enum::enum_name(a.kind) };
	auto b_kind{ magic_enum::enum_name(b.kind) };

	if (sort_mode == ContentBrowserPanel::SortMode::Type) {
		if (a_kind != b_kind) {
			return a_kind < b_kind;
		}
		return a.key < b.key;
	}

	if (a.key != b.key) {
		return a.key < b.key;
	}

	return a_kind < b_kind;
}

void DrawAssetTileContent(
	float tile_width, const impl::AssetRecord& asset, impl::TextureId audio_icon,
	impl::TextureId document_icon, std::optional<impl::AssetRecord>& asset_to_unload
) {
	ImGui::PushID(asset.key.value.c_str());
	ImGui::BeginGroup();

	auto key{ asset.key.value };
	auto key_text{ key };
	bool is_default_font{ key == kDefaultFont };
	bool unloadable_asset{ !is_default_font };

	if (is_default_font) {
		key_text = "Default Font";
	}

	DrawPreview(tile_width, GetTilePreview(asset, audio_icon, document_icon));

	BeginAssetKeyDragDropSource(key, asset.kind, key_text);

	if (ImGui::BeginPopupContextItem("##asset_context")) {
		ImGui::BeginDisabled(!unloadable_asset);

		if (ImGui::MenuItem("Unload")) {
			asset_to_unload = asset;
		}

		ImGui::EndDisabled();
		ImGui::EndPopup();
	}

	auto kind_text{ magic_enum::enum_name(asset.kind) };
	DrawWrappedText(kind_text, tile_width, true);

	DrawWrappedText(key_text, tile_width);
	BeginAssetKeyDragDropSource(key, asset.kind, key_text);

	if (!asset.source_path.empty()) {
		auto filename{ asset.source_path.filename().string() };
		DrawWrappedText(filename, tile_width, true);
	} else {
		DrawWrappedText("Generated asset", tile_width, true);
	}

	ImGui::EndGroup();
	ImGui::PopID();
}

void DrawAssetTileUnloadButton(
	float tile_width, const impl::AssetRecord& asset,
	std::optional<impl::AssetRecord>& asset_to_unload
) {
	ImGui::PushID(asset.key.value.c_str());

	bool unloadable_asset{ asset.key.value != kDefaultFont };

	ImGui::BeginDisabled(!unloadable_asset);

	if (ImGui::Button("Unload", ImVec2{ tile_width, 0.0f })) {
		asset_to_unload = asset;
	}

	ImGui::EndDisabled();

	ImGui::Dummy(ImVec2{ 0.0f, ImGui::GetStyle().FramePadding.y });

	ImGui::PopID();
}

} // namespace

bool AcceptAssetKeyDragDrop(AssetKey& value, std::optional<AssetKind> accepted_kind) {
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}

	bool changed{ false };

	const auto* payload{ ImGui::GetDragDropPayload() };

	if (payload != nullptr && payload->IsDataType(kAssetKeyPayloadType) &&
		payload->DataSize == sizeof(AssetKeyPayload)) {
		const auto* asset_payload{ static_cast<const AssetKeyPayload*>(payload->Data) };

		bool accepts_kind{ !accepted_kind.has_value() || *accepted_kind == asset_payload->kind };

		if (accepts_kind) {
			if (ImGui::AcceptDragDropPayload(kAssetKeyPayloadType) != nullptr) {
				value.value = asset_payload->key;
				changed		= true;
			}
		}
	}

	ImGui::EndDragDropTarget();

	return changed;
}

std::vector<path> ContentBrowserPanel::ConsumeDroppedFiles() {
	auto dropped_files{ std::move(dropped_files_) };
	dropped_files_.clear();

	return dropped_files;
}

void ContentBrowserPanel::AddDroppedFile(const path& file_path) {
	dropped_files_.push_back(file_path);
}

void ContentBrowserPanel::InitializePreviewIcons(EditorContext& ctx) {
	if (preview_icons_initialized_) {
		return;
	}

	audio_icon_texture_ =
		CreateEmbeddedIconTexture(ctx.editor.GetRenderer(), std::span{ embedded::kAudioNotePng });
	document_icon_texture_ =
		CreateEmbeddedIconTexture(ctx.editor.GetRenderer(), std::span{ embedded::kDocumentPng });

	preview_icons_initialized_ = true;
}

void ContentBrowserPanel::OnRender(EditorContext& ctx) {
	if (ImGui::Begin("Content Browser")) {
		DrawContentBrowser(ctx);
	}
	ImGui::End();

	if (ImGui::Begin("Render Stats")) {
		auto& stats{ ctx.editor.GetDebugSystem().stats };
		auto draw_calls{ "Draw calls: " + ToString(stats.GetCount("draw_calls")) };
		ImGui::TextUnformatted(draw_calls.c_str());
	}
	ImGui::End();
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
		auto dropped_files{ ConsumeDroppedFiles() };
		if (!dropped_files.empty()) {
			ImportAssetPaths(ctx, dropped_files, status_);
		}
	}

	DrawAssetGrid(ctx);
}

void ContentBrowserPanel::DrawToolbar(EditorContext& ctx) {
	if (ImGui::Button("Import...", ImVec2{ 120.0f, 0.0f })) {
		auto result{ ctx.editor.GetWindow().file.OpenFiles(ImportOptions()) };

		if (!result.has_value()) {
			status_ = "Import failed: " + result.error();
		} else if (result->has_value()) {
			ImportAssetPaths(ctx, result->value(), status_);
		}
	}

	ImGui::SameLine();

	ImGui::TextUnformatted("Items per row");
	ImGui::SameLine();

	ImGui::SetNextItemWidth(80.0f);
	ImGui::InputInt("##items_per_row", &items_per_row_, 1, 1);

	items_per_row_ = std::clamp(items_per_row_, 1, kMaxItemsPerRow);

	ImGui::SameLine();
	ImGui::TextUnformatted("Sort by");
	ImGui::SameLine();

	const char* sort_mode_name{ sort_mode_ == SortMode::Name ? "Name" : "Type" };

	ImGui::SetNextItemWidth(100.0f);
	if (ImGui::BeginCombo("##sort_mode", sort_mode_name)) {
		if (ImGui::Selectable("Name", sort_mode_ == SortMode::Name)) {
			sort_mode_ = SortMode::Name;
		}
		if (ImGui::Selectable("Type", sort_mode_ == SortMode::Type)) {
			sort_mode_ = SortMode::Type;
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();

	if (DrawSortDirectionButton(
			"##sort_ascending", ImGuiDir_Up, sort_ascending_, "Sort ascending"
		)) {
		sort_ascending_ = true;
	}

	ImGui::SameLine(0.0f, 1.0f);

	if (DrawSortDirectionButton(
			"##sort_descending", ImGuiDir_Down, !sort_ascending_, "Sort descending"
		)) {
		sort_ascending_ = false;
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##content_browser_search", "Search assets...", &search_);
}

void ContentBrowserPanel::DrawAssetGrid(EditorContext& ctx) {
	auto assets{ impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	std::erase_if(assets, [&](const auto& asset) { return !MatchesSearch(asset, search_); });

	std::stable_sort(assets.begin(), assets.end(), [&](const auto& a, const auto& b) {
		return sort_ascending_ ? AssetLess(a, b, sort_mode_) : AssetLess(b, a, sort_mode_);
	});

	ImGui::BeginChild(
		"##ContentBrowserScrollRegion", ImVec2{ 0.0f, 0.0f }, false,
		ImGuiWindowFlags_AlwaysVerticalScrollbar
	);

	if (assets.empty()) {
		ImGui::TextDisabled("No assets. Import files or drop them here.");
		ImGui::EndChild();
		return;
	}

	auto column_count{ std::clamp(items_per_row_, 1, kMaxItemsPerRow) };

	std::optional<impl::AssetRecord> asset_to_unload;

	if (ImGui::BeginTable(
			"##ContentBrowserGrid", column_count,
			ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoBordersInBody |
				ImGuiTableFlags_PadOuterX
		)) {
		for (auto i{ 0 }; i < column_count; ++i) {
			ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch, 1.0f);
		}

		auto assets_per_row{ static_cast<std::size_t>(column_count) };

		for (auto row_begin{ 0uz }; row_begin < assets.size(); row_begin += assets_per_row) {
			auto row_asset_count{ std::min(assets_per_row, assets.size() - row_begin) };

			// The table uses the tallest content cell as the height of this row.
			ImGui::TableNextRow();

			for (auto column{ 0uz }; column < row_asset_count; ++column) {
				ImGui::TableSetColumnIndex(static_cast<int>(column));

				auto tile_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				auto& asset{ assets[row_begin + column] };

				DrawAssetTileContent(
					tile_width, asset, static_cast<impl::TextureId>(audio_icon_texture_),
					static_cast<impl::TextureId>(document_icon_texture_), asset_to_unload
				);
			}

			// Starting another table row places every button below the tallest content
			// cell from the previous row, leaving blank space in shorter cells.
			ImGui::TableNextRow();

			for (auto column{ 0uz }; column < row_asset_count; ++column) {
				ImGui::TableSetColumnIndex(static_cast<int>(column));

				auto tile_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
				auto& asset{ assets[row_begin + column] };

				DrawAssetTileUnloadButton(tile_width, asset, asset_to_unload);
			}
		}

		ImGui::EndTable();
	}

	if (asset_to_unload.has_value()) {
		if (impl::AssetAccessor{ ctx.editor.GetAssetManager() }.Unload(
				asset_to_unload->key, asset_to_unload->kind
			)) {
			status_ = "Unloaded " + asset_to_unload->key.value;
		} else {
			status_ = "Failed to unload " + asset_to_unload->key.value;
		}
	}

	ImGui::EndChild();
}

} // namespace ptgn::editor
