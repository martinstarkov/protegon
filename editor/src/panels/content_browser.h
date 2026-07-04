#pragma once

#include <optional>
#include <string>
#include <vector>

#include "core/util/file.h"
#include "runtime/asset/asset_manager.h"

namespace ptgn::editor {

class EditorContext;

bool AcceptAssetKeyDragDrop(
	std::string& value, std::optional<impl::AssetKind> accepted_kind = std::nullopt
);

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