#include "panels/content_browser.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <span>
#include <string>
#include <vector>

#include "core/editor.h"
#include "core/editor_context.h"
#include "core/util/string.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "runtime/asset/asset_manager.h"
#include "tools/debug/debug_system.h"
#include "tools/debug/stats.h"

namespace ptgn::editor {

namespace {

inline constexpr char kAssetKeyPayloadType[]{ "PTGN_ASSET_KEY" };

struct AssetKeyPayload {
	impl::AssetKind kind{ impl::AssetKind::Unknown };
	char key[256]{};
};

inline void BeginAssetKeyDragDropSource(std::string_view key, impl::AssetKind kind) {
	if (!ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
		return;
	}

	AssetKeyPayload payload;
	payload.kind = kind;

	auto length{ std::min(key.size(), sizeof(payload.key) - 1) };
	std::memcpy(payload.key, key.data(), length);
	payload.key[length] = '\0';

	ImGui::SetDragDropPayload(kAssetKeyPayloadType, &payload, sizeof(payload));
	ImGui::TextUnformatted(payload.key);

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

std::string DefaultAssetKey(const path& asset_path) {
	auto key{ asset_path.stem().string() };
	return SanitizeAssetKey(key);
}

std::string MakeUniqueAssetKey(AssetManager& assets, const path& asset_path) {
	auto base_key{ DefaultAssetKey(asset_path) };
	auto key{ base_key };

	for (auto suffix{ 1 }; assets.Has(key); ++suffix) {
		key = base_key + "_" + std::to_string(suffix);
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

		if (kind == impl::AssetKind::Unknown) {
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

	if (ContainsInsensitive(asset.key, search)) {
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

void DrawAssetTile(
	float preview_size, const impl::AssetRecord& asset,
	std::optional<impl::AssetRecord>& asset_to_unload
) {
	ImGui::PushID(asset.key.value.c_str());

	ImGui::BeginGroup();

	if (std::string name{ magic_enum::enum_name(asset.kind) };
		ImGui::Button(name.c_str(), ImVec2{ preview_size, preview_size })) {}

	BeginAssetKeyDragDropSource(asset.key, asset.kind);

	if (ImGui::BeginPopupContextItem("##asset_context")) {
		if (ImGui::MenuItem("Unload")) {
			asset_to_unload = asset;
		}

		ImGui::EndPopup();
	}

	auto key_text{ asset.key.value };
	ImGui::SetNextItemWidth(preview_size);
	ImGui::InputText("##key", &key_text, ImGuiInputTextFlags_ReadOnly);
	BeginAssetKeyDragDropSource(asset.key.value, asset.kind);

	if (!asset.source_path.empty()) {
		auto filename{ asset.source_path.filename().string() };

		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + preview_size);
		ImGui::TextDisabled("%s", filename.c_str());
		ImGui::PopTextWrapPos();
	} else {
		ImGui::TextDisabled("Generated asset");
	}

	if (ImGui::SmallButton("Unload")) {
		asset_to_unload = asset;
	}

	ImGui::EndGroup();

	ImGui::PopID();
}

} // namespace

bool AcceptAssetKeyDragDrop(std::string& value, std::optional<impl::AssetKind> accepted_kind) {
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}

	bool changed{ false };

	auto* payload{ ImGui::AcceptDragDropPayload(kAssetKeyPayloadType) };
	if (payload != nullptr && payload->DataSize == sizeof(AssetKeyPayload)) {
		auto* asset_payload{ static_cast<const AssetKeyPayload*>(payload->Data) };

		if (!accepted_kind.has_value() || *accepted_kind == asset_payload->kind) {
			value	= asset_payload->key;
			changed = true;
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

	ImGui::SetNextItemWidth(180.0f);
	ImGui::SliderInt("##items_per_row", &items_per_row_, 1, 8, "Items Per Row: %d");

	ImGui::SameLine();

	if (ImGui::Button("Sort: Name")) {
		if (sort_mode_ == SortMode::Name) {
			sort_ascending_ = !sort_ascending_;
		} else {
			sort_mode_		= SortMode::Name;
			sort_ascending_ = true;
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("Sort: Type")) {
		if (sort_mode_ == SortMode::Type) {
			sort_ascending_ = !sort_ascending_;
		} else {
			sort_mode_		= SortMode::Type;
			sort_ascending_ = true;
		}
	}

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##content_browser_search", "Search assets...", &search_);
}

void ContentBrowserPanel::DrawAssetGrid(EditorContext& ctx) {
	auto assets{ impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	std::erase_if(assets, [&](const auto& asset) { return !MatchesSearch(asset, search_); });

	std::stable_sort(assets.begin(), assets.end(), [&](const auto& a, const auto& b) {
		bool result{ false };

		if (sort_mode_ == SortMode::Type) {
			auto a_kind{ magic_enum::enum_name(a.kind) };
			auto b_kind{ magic_enum::enum_name(b.kind) };

			if (a_kind != b_kind) {
				result = a_kind < b_kind;
			} else {
				result = a.key < b.key;
			}
		} else {
			result = a.key < b.key;
		}

		return sort_ascending_ ? result : !result;
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

	auto& style{ ImGui::GetStyle() };

	auto tile_padding{ 10.0f };
	auto available_width{ ImGui::GetContentRegionAvail().x };
	auto column_count{ std::max(1, items_per_row_) };
	auto total_spacing{ static_cast<float>(column_count - 1) * tile_padding };
	auto tile_width{ (available_width - total_spacing) / static_cast<float>(column_count) };
	auto preview_size{ std::max(48.0f, tile_width) };

	std::optional<impl::AssetRecord> asset_to_unload;

	if (ImGui::BeginTable(
			"##ContentBrowserGrid", column_count,
			ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoBordersInBody |
				ImGuiTableFlags_PadOuterX
		)) {
		for (auto& asset : assets) {
			ImGui::TableNextColumn();

			auto start_y{ ImGui::GetCursorPosY() };

			DrawAssetTile(preview_size, asset, asset_to_unload);

			auto used_height{ ImGui::GetCursorPosY() - start_y };
			auto minimum_height{ preview_size + ImGui::GetTextLineHeightWithSpacing() * 4.0f };

			auto extra_height{ minimum_height - used_height + style.FramePadding.y };

			if (extra_height > 0.0f) {
				ImGui::Dummy(ImVec2{ 0.0f, extra_height });
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