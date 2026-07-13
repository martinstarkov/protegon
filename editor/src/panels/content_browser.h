#pragma once

#include <concepts>
#include <optional>
#include <string>
#include <type_traits>
#include <vector>

#include "core/util/file.h"
#include "runtime/asset/asset_key.h"

namespace ptgn::editor {

class EditorContext;

bool AcceptAssetKeyDragDrop(AssetKey& value, std::optional<AssetKind> accepted_kind = std::nullopt);

template <typename T>
concept SpecificAssetKey =
	std::derived_from<std::remove_cvref_t<T>, AssetKey> &&
	!std::same_as<std::remove_cvref_t<T>, AssetKey> && requires { std::remove_cvref_t<T>::kind; };

template <SpecificAssetKey T>
bool AcceptAssetKeyDragDrop(T& value) {
	using Value = std::remove_cvref_t<T>;

	return AcceptAssetKeyDragDrop(static_cast<AssetKey&>(value), Value::kind);
}

class ContentBrowserPanel {
public:
	void OnRender(EditorContext& ctx);

private:
	enum class SortMode {
		Name,
		Type,
	};

	void DrawContentBrowser(EditorContext& ctx);
	void DrawToolbar(EditorContext& ctx);
	void DrawAssetGrid(EditorContext& ctx);

	[[nodiscard]] std::vector<path> ConsumeDroppedFiles();
	void AddDroppedFile(const path& file_path);

	std::vector<path> dropped_files_;
	int items_per_row_{ 4 };
	SortMode sort_mode_{ SortMode::Name };
	bool sort_ascending_{ true };
	std::string search_;
	std::string status_;
};

} // namespace ptgn::editor