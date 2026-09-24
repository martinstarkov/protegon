#include "editor/paint/paint_editor.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <memory>
#include <ranges>
#include <regex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include "core/assert.h"
#include "core/math/angle.h"
#include "core/math/geometry/rect.h"
#include "editor/color_picker.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

namespace {

constexpr ImU32 kSelection{ IM_COL32(78, 190, 255, 235) };
constexpr ImU32 kPreview{ IM_COL32(255, 208, 70, 220) };
constexpr ImU32 kLockedPreview{ IM_COL32(255, 92, 92, 180) };
constexpr std::size_t kMaxFloodCells{ 50000 };
constexpr const char* kTileLibraryEntryDragDropPayload{ "PTGN_TILE_LIBRARY_ENTRY" };

[[nodiscard]] float FractalNoise01(
	V2_float world, NoiseType type, int seed, float frequency, int octaves, float lacunarity,
	float persistence, V2_float offset
);

template <typename Range>
[[nodiscard]] std::string NextDefaultGroupName(const Range& groups) {
	for (std::size_t index{ 1 };; ++index) {
		std::string candidate{ "Group " + std::to_string(index) };
		if (!std::ranges::contains(groups, candidate)) {
			return candidate;
		}
	}
}

[[nodiscard]] ImVec2 ToImGui(V2_float value) {
	return { value.x, value.y };
}

[[nodiscard]] V2_float FromImGui(ImVec2 value) {
	return { value.x, value.y };
}

[[nodiscard]] bool Contains(Viewport viewport, V2_float point) {
	return point.x >= viewport.position.x && point.y >= viewport.position.y &&
		   point.x < viewport.position.x + viewport.size.x &&
		   point.y < viewport.position.y + viewport.size.y;
}

[[nodiscard]] V2_float ScreenToWorld(
	V2_float screen, const FrameContext& frame, Viewport presentation_viewport
) {
	const V2_float presentation{ screen - presentation_viewport.GetCenter() };
	return ConvertPoint(presentation, Frame::Presentation, Frame::World, frame);
}

[[nodiscard]] V2_float WorldToScreen(
	V2_float world, const FrameContext& frame, Viewport presentation_viewport
) {
	return ConvertPoint(world, Frame::World, Frame::Presentation, frame) +
		   presentation_viewport.GetCenter();
}

struct PaintSelectionRect {
	V2_float min{};
	V2_float max{};
};

[[nodiscard]] bool Contains(const PaintSelectionRect& rect, V2_float point) {
	return point.x >= rect.min.x && point.x <= rect.max.x &&
		point.y >= rect.min.y && point.y <= rect.max.y;
}

[[nodiscard]] bool Overlaps(const PaintSelectionRect& a, const PaintSelectionRect& b) {
	return a.min.x <= b.max.x && a.max.x >= b.min.x &&
		a.min.y <= b.max.y && a.max.y >= b.min.y;
}

[[nodiscard]] PaintSelectionRect MakeSelectionRect(V2_float a, V2_float b) {
	return {
		.min = { std::min(a.x, b.x), std::min(a.y, b.y) },
		.max = { std::max(a.x, b.x), std::max(a.y, b.y) },
	};
}

template <typename Range>
[[nodiscard]] std::optional<PaintSelectionRect> BoundsFromVertices(const Range& vertices) {
	if (std::ranges::empty(vertices)) {
		return std::nullopt;
	}

	auto it{ std::ranges::begin(vertices) };
	PaintSelectionRect result{ .min = *it, .max = *it };
	for (++it; it != std::ranges::end(vertices); ++it) {
		result.min.x = std::min(result.min.x, it->x);
		result.min.y = std::min(result.min.y, it->y);
		result.max.x = std::max(result.max.x, it->x);
		result.max.y = std::max(result.max.y, it->y);
	}
	return result;
}

void MergeBounds(std::optional<PaintSelectionRect>& target, const PaintSelectionRect& source) {
	if (!target.has_value()) {
		target = source;
		return;
	}
	target->min.x = std::min(target->min.x, source.min.x);
	target->min.y = std::min(target->min.y, source.min.y);
	target->max.x = std::max(target->max.x, source.max.x);
	target->max.y = std::max(target->max.y, source.max.y);
}

[[nodiscard]] std::optional<PaintSelectionRect> EntityOwnSelectionBounds(Entity entity) {
	if (!entity || !entity.Has<Transform>() || !IsVisible(entity)) {
		return std::nullopt;
	}

	auto transform{ GetDrawTransform(entity) };
	const Origin origin{ entity.GetOrDefault<Origin>() };

	if (entity.Has<Rect>()) {
		return BoundsFromVertices(entity.Get<Rect>().GetWorldVertices(transform, origin));
	}

	if (entity.Has<Circle>()) {
		return BoundsFromVertices(
			Rect{ entity.Get<Circle>().GetSize() }.GetWorldVertices(transform, origin)
		);
	}

	if (const auto display_size{ GetDisplaySize(entity) };
		display_size.has_value() && display_size->IsPositive()) {
		// Sprite drawing normalizes absolute transform scale because GetDisplaySize() already
		// incorporates it. Mirror that here so hit-testing matches the rendered rectangle.
		transform.scale.x = transform.scale.x < 0.0f ? -1.0f : 1.0f;
		transform.scale.y = transform.scale.y < 0.0f ? -1.0f : 1.0f;
		return BoundsFromVertices(Rect{ *display_size }.GetWorldVertices(transform, origin));
	}

	if (entity.Has<::ptgn::impl::TextData>()) {
		const V2_float size{ Text{ entity }.GetSize() };
		if (size.IsPositive()) {
			return BoundsFromVertices(Rect{ size }.GetWorldVertices(transform, origin));
		}
	}

	return std::nullopt;
}

[[nodiscard]] std::optional<PaintSelectionRect> EntitySelectionBounds(
	Entity entity, std::size_t depth = 0
) {
	constexpr std::size_t kMaximumSelectionHierarchyDepth{ 64 };
	if (!entity || depth >= kMaximumSelectionHierarchyDepth || !IsVisible(entity)) {
		return std::nullopt;
	}

	std::optional<PaintSelectionRect> result{ EntityOwnSelectionBounds(entity) };
	if (HasChildren(entity)) {
		for (Entity child : GetChildren(entity)) {
			if (const auto child_bounds{ EntitySelectionBounds(child, depth + 1) }) {
				MergeBounds(result, *child_bounds);
			}
		}
	}
	return result;
}

[[nodiscard]] PaintSelectionRect TileSelectionBounds(Tilemap map, const TilemapTile& tile) {
	const auto& data{ map.GetData() };
	const V2_float visual_size{
		tile.pixel_size.IsPositive() ? V2_float{ tile.pixel_size } : data.cell_size
	};
	const V2_float anchor{ map.CellToWorld(tile.coordinate) + tile.offset };
	const V2_float center{ anchor + GetOffset(tile.origin, visual_size) };
	return {
		.min = center - visual_size * 0.5f,
		.max = center + visual_size * 0.5f,
	};
}

[[nodiscard]] V2_float ConstrainSquareDrag(V2_float start, V2_float current) {
	const float dx{ current.x - start.x };
	const float dy{ current.y - start.y };
	const float side{ std::max(std::abs(dx), std::abs(dy)) };
	return {
		start.x + (dx < 0.0f ? -side : side),
		start.y + (dy < 0.0f ? -side : side),
	};
}

[[nodiscard]] std::optional<PaintSelectionRect> GeneratorSelectionBounds(Entity entity) {
	if (!entity || !IsPaintGenerator(entity)) {
		return std::nullopt;
	}
	PaintGenerator generator{ entity };
	const auto& data{ generator.GetData() };
	if (!data.enabled || !data.grid_size.IsPositive()) {
		return std::nullopt;
	}

	const V2_float entity_offset{ GetWorldPosition(generator) };
	const V2_float grid_origin{ data.grid_offset + entity_offset };
	auto to_cell = [&](V2_float point) {
		return V2_int{
			static_cast<int>(std::floor((point.x - grid_origin.x) / data.grid_size.x)),
			static_cast<int>(std::floor((point.y - grid_origin.y) / data.grid_size.y)),
		};
	};

	if (data.geometry == PaintGeneratorGeometry::Infinite) {
		return PaintSelectionRect{
			.min = { -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() },
			.max = { std::numeric_limits<float>::max(), std::numeric_limits<float>::max() },
		};
	}

	std::optional<V2_int> first;
	std::optional<V2_int> last;
	auto include = [&](V2_int cell) {
		if (!first.has_value()) {
			first = last = cell;
			return;
		}
		first->x = std::min(first->x, cell.x);
		first->y = std::min(first->y, cell.y);
		last->x = std::max(last->x, cell.x);
		last->y = std::max(last->y, cell.y);
	};

	if (data.geometry == PaintGeneratorGeometry::Rectangle) {
		const V2_int a{ to_cell(data.start + entity_offset) };
		const V2_int b{ to_cell(data.end + entity_offset) };
		include({ std::min(a.x, b.x), std::min(a.y, b.y) });
		include({ std::max(a.x, b.x), std::max(a.y, b.y) });
	} else if (data.geometry == PaintGeneratorGeometry::Line) {
		V2_int a{ to_cell(data.start + entity_offset) };
		const V2_int b{ to_cell(data.end + entity_offset) };
		const int dx{ std::abs(b.x - a.x) };
		const int dy{ -std::abs(b.y - a.y) };
		const int sx{ a.x < b.x ? 1 : -1 };
		const int sy{ a.y < b.y ? 1 : -1 };
		const int half{ std::max(1, data.line_thickness) / 2 };
		int error{ dx + dy };
		for (;;) {
			include(a - V2_int{ half, half });
			include(a + V2_int{ half, half });
			if (a == b) {
				break;
			}
			const int e2{ 2 * error };
			if (e2 >= dy) {
				error += dy;
				a.x += sx;
			}
			if (e2 <= dx) {
				error += dx;
				a.y += sy;
			}
		}
	} else {
		for (const auto& point : data.stroke_points) {
			const V2_int center{ to_cell(point.position + entity_offset) };
			const int diameter{ std::max(1, point.diameter) };
			const int low{ -(diameter / 2) };
			const int high{ low + diameter - 1 };
			include(center + V2_int{ low, low });
			include(center + V2_int{ high, high });
		}
	}

	if (!first.has_value() || !last.has_value()) {
		return std::nullopt;
	}
	return PaintSelectionRect{
		.min = grid_origin + V2_float{ static_cast<float>(first->x), static_cast<float>(first->y) } * data.grid_size,
		.max = grid_origin + V2_float{ static_cast<float>(last->x + 1), static_cast<float>(last->y + 1) } * data.grid_size,
	};
}

[[nodiscard]] const char* ToolName(PaintTool tool) {
	switch (tool) {
		case PaintTool::Select:		return "Select";
		case PaintTool::Move:		return "Move";
		case PaintTool::Pencil:		return "Pencil";
		case PaintTool::Brush:		return "Brush";
		case PaintTool::Line:		return "Line";
		case PaintTool::Rectangle:	return "Rectangle";
		case PaintTool::Fill:		return "Fill";
		case PaintTool::Erase:		return "Erase";
		case PaintTool::Eyedropper: return "Eyedropper";
	}
	return "Paint";
}

[[nodiscard]] const char* ToolShortcut(PaintTool tool) {
	switch (tool) {
		case PaintTool::Select:		return "S";
		case PaintTool::Move:		return "M";
		case PaintTool::Pencil:		return "P";
		case PaintTool::Brush:		return "B";
		case PaintTool::Line:		return "L";
		case PaintTool::Rectangle:	return "R";
		case PaintTool::Fill:		return "F";
		case PaintTool::Erase:		return "E";
		case PaintTool::Eyedropper: return "K";
	}
	return "";
}

[[nodiscard]] bool CtrlDown() {
	return ImGui::GetIO().KeyCtrl || ImGui::GetIO().KeySuper;
}

[[nodiscard]] std::uint32_t HashCell(V2_int cell, std::uint32_t seed = 0x9e3779b9u) {
	std::uint32_t x{ static_cast<std::uint32_t>(cell.x) };
	std::uint32_t y{ static_cast<std::uint32_t>(cell.y) };
	std::uint32_t h{ seed ^ (x * 0x85ebca6bu) ^ (y * 0xc2b2ae35u) };
	h ^= h >> 16u;
	h *= 0x7feb352du;
	h ^= h >> 15u;
	h *= 0x846ca68bu;
	h ^= h >> 16u;
	return h;
}

[[nodiscard]] float Hash01(V2_int cell) {
	return static_cast<float>(HashCell(cell) & 0x00ffffffu) / static_cast<float>(0x01000000u);
}

[[nodiscard]] int FloorMod(int value, int divisor) {
	if (divisor <= 0) {
		return 0;
	}
	const int result{ value % divisor };
	return result < 0 ? result + divisor : result;
}

[[nodiscard]] bool GeneratorContainsWorld(Entity entity, V2_float world) {
	if (!entity || !IsPaintGenerator(entity)) {
		return false;
	}
	PaintGenerator generator{ entity };
	const auto& data{ generator.GetData() };
	if (!data.enabled || !data.grid_size.IsPositive()) {
		return false;
	}

	const V2_float entity_offset{ GetWorldPosition(generator) };
	const V2_float origin{ data.grid_offset + entity_offset };
	auto to_cell = [&](V2_float point) {
		return V2_int{
			static_cast<int>(std::floor((point.x - origin.x) / data.grid_size.x)),
			static_cast<int>(std::floor((point.y - origin.y) / data.grid_size.y)),
		};
	};

	const V2_int target{ to_cell(world) };
	if (generator.IsSuppressed(target)) {
		return false;
	}
	if (data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity &&
		Hash01(target) > data.recipe.density) {
		return false;
	}

	if (data.geometry == PaintGeneratorGeometry::Infinite) {
		return true;
	}

	if (data.geometry == PaintGeneratorGeometry::Rectangle) {
		const V2_int a{ to_cell(data.start + entity_offset) };
		const V2_int b{ to_cell(data.end + entity_offset) };
		const int min_x{ std::min(a.x, b.x) };
		const int max_x{ std::max(a.x, b.x) };
		const int min_y{ std::min(a.y, b.y) };
		const int max_y{ std::max(a.y, b.y) };
		if (target.x < min_x || target.x > max_x || target.y < min_y || target.y > max_y) {
			return false;
		}
		const int l{ target.x - min_x };
		const int r{ max_x - target.x };
		const int t{ target.y - min_y };
		const int bottom{ max_y - target.y };
		switch (data.area_mode) {
			case PaintGeneratorAreaMode::Fill: return true;
			case PaintGeneratorAreaMode::RandomFill:
				return Hash01(target) <= data.random_fill_density;
			case PaintGeneratorAreaMode::Outline:
				return l < data.area_thickness || r < data.area_thickness ||
					t < data.area_thickness || bottom < data.area_thickness;
			case PaintGeneratorAreaMode::Corners: {
				const int q{ std::max(1, data.area_thickness) };
				return (l < q && t < q) || (r < q && t < q) ||
					(l < q && bottom < q) || (r < q && bottom < q);
			}
		}
	}

	if (data.geometry == PaintGeneratorGeometry::Line) {
		V2_int a{ to_cell(data.start + entity_offset) };
		const V2_int b{ to_cell(data.end + entity_offset) };
		const int dx{ std::abs(b.x - a.x) };
		const int dy{ -std::abs(b.y - a.y) };
		const int sx{ a.x < b.x ? 1 : -1 };
		const int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};
		for (;;) {
			if (step % std::max(1, data.line_spacing) == 0) {
				const int half{ std::max(1, data.line_thickness) / 2 };
				if (target.x >= a.x - half && target.x <= a.x + half &&
					target.y >= a.y - half && target.y <= a.y + half) {
					return true;
				}
			}
			if (a == b) {
				break;
			}
			const int e2{ 2 * error };
			if (e2 >= dy) {
				error += dy;
				a.x += sx;
			}
			if (e2 <= dx) {
				error += dx;
				a.y += sy;
			}
			++step;
		}
		return false;
	}

	for (const auto& point : data.stroke_points) {
		const V2_int center{ to_cell(point.position + entity_offset) };
		const int diameter{ std::max(1, point.diameter) };
		const int low{ -(diameter / 2) };
		const int high{ low + diameter - 1 };
		const V2_int delta{ target - center };
		if (delta.x < low || delta.x > high || delta.y < low || delta.y > high) {
			continue;
		}
		if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
			const float dx{ static_cast<float>(delta.x) + 0.5f };
			const float dy{ static_cast<float>(delta.y) + 0.5f };
			if (std::sqrt(dx * dx + dy * dy) > static_cast<float>(diameter) * 0.5f) {
				continue;
			}
		}
		return true;
	}
	return false;
}

template <typename Entry, typename Weight>
[[nodiscard]] std::optional<std::size_t> ChooseWeightedIndex(
	const std::vector<Entry>& entries, V2_int cell, Weight weight
) {
	float total{};
	for (const auto& entry : entries) {
		total += std::max(0.0f, weight(entry));
	}
	if (entries.empty()) {
		return std::nullopt;
	}
	if (total <= 0.0f) {
		return std::size_t{ 0 };
	}
	float choice{ Hash01(cell) * total };
	for (std::size_t i{}; i < entries.size(); ++i) {
		choice -= std::max(0.0f, weight(entries[i]));
		if (choice <= 0.0f) {
			return i;
		}
	}
	return entries.size() - 1;
}

[[nodiscard]] int CardinalOccupancyMask(const std::function<bool(V2_int)>& occupied, V2_int cell) {
	int mask{};
	if (occupied(cell + V2_int{ 0, -1 })) {
		mask |= 1;
	}
	if (occupied(cell + V2_int{ 1, 0 })) {
		mask |= 2;
	}
	if (occupied(cell + V2_int{ 0, 1 })) {
		mask |= 4;
	}
	if (occupied(cell + V2_int{ -1, 0 })) {
		mask |= 8;
	}
	return mask;
}

[[nodiscard]] bool IsProtectedSceneEntity(Scene& scene, Entity entity) {
	return entity == scene.GetRenderTarget() || entity == scene.GetCamera() ||
		   entity == scene.GetFixedCamera();
}

[[nodiscard]] Entity RestoreEntityTree(
	Scene& scene, const SerializedEntity& input, SceneLayerId layer, Entity parent = {}
) {
	Entity entity{ input.uuid.has_value() ? scene.CreateEntity(Tag{ input.tag }, *input.uuid)
										  : scene.CreateEntity(Tag{ input.tag }) };
	DeserializeEntity(input, entity);
	if (parent) {
		SetParent(entity, parent);
	}
	for (const auto& child : input.children) {
		static_cast<void>(RestoreEntityTree(scene, child, layer, entity));
	}
	if (!parent) {
		scene.GetLayers().Assign(entity, layer, true);
	}
	return entity;
}

[[nodiscard]] std::string TextureDisplayName(const TextureKey& key) {
	std::string name{ key.value };
	const auto slash{ name.find_last_of("/\\") };
	if (slash != std::string::npos) {
		name.erase(0, slash + 1);
	}
	return name;
}

[[nodiscard]] std::string PrefabDisplayName(const PrefabKey& key) {
	std::string name{ key.value };
	if (name.starts_with(kPrefabKeyPrefix)) {
		name.erase(0, kPrefabKeyPrefix.size());
	}
	const auto slash{ name.find_last_of("/\\") };
	if (slash != std::string::npos) {
		name.erase(0, slash + 1);
	}
	return name.empty() ? key.value : name;
}

void SortGroupsUngroupedFirst(std::vector<std::string>& groups) {
	std::ranges::sort(groups, [](const std::string& a, const std::string& b) {
		if ((a == "Ungrouped") != (b == "Ungrouped")) {
			return a == "Ungrouped";
		}
		return a < b;
	});
	groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
}

[[nodiscard]] int RequiredAutotileTileCount(PaintAutotileFormat format) {
	switch (format) {
		case PaintAutotileFormat::Classic15:  return 15;
		case PaintAutotileFormat::Blob47:	  return 47;
		case PaintAutotileFormat::Subset16:
		case PaintAutotileFormat::DualGrid16:
		case PaintAutotileFormat::Wang16:	  return 16;
	}
	return 16;
}

[[nodiscard]] const char* AutotileFormatName(PaintAutotileFormat format) {
	switch (format) {
		case PaintAutotileFormat::Classic15:  return "Classic 15";
		case PaintAutotileFormat::Blob47:	  return "Blob 47 (8-neighbor)";
		case PaintAutotileFormat::Subset16:	  return "4-neighbor / Subset 16";
		case PaintAutotileFormat::DualGrid16: return "Dual Grid 16";
		case PaintAutotileFormat::Wang16:	  return "Wang 16";
	}
	return "Autotile";
}

[[nodiscard]] int BlobOccupancyMask(const std::function<bool(V2_int)>& occupied, V2_int c) {
	const bool n{ occupied(c + V2_int{ 0, -1 }) };
	const bool e{ occupied(c + V2_int{ 1, 0 }) };
	const bool south{ occupied(c + V2_int{ 0, 1 }) };
	const bool w{ occupied(c + V2_int{ -1, 0 }) };
	int mask{};
	if (n) {
		mask |= 1;
	}
	if (e) {
		mask |= 2;
	}
	if (south) {
		mask |= 4;
	}
	if (w) {
		mask |= 8;
	}
	if (n && e && occupied(c + V2_int{ 1, -1 })) {
		mask |= 16;
	}
	if (e && south && occupied(c + V2_int{ 1, 1 })) {
		mask |= 32;
	}
	if (south && w && occupied(c + V2_int{ -1, 1 })) {
		mask |= 64;
	}
	if (w && n && occupied(c + V2_int{ -1, -1 })) {
		mask |= 128;
	}
	return mask;
}

[[nodiscard]] const std::vector<int>& ValidBlob47Masks() {
	static const std::vector<int> masks = [] {
		std::vector<int> result;
		for (int mask{}; mask < 256; ++mask) {
			const bool n{ (mask & 1) != 0 };
			const bool e{ (mask & 2) != 0 };
			const bool south{ (mask & 4) != 0 };
			const bool w{ (mask & 8) != 0 };
			if ((mask & 16) && !(n && e)) {
				continue;
			}
			if ((mask & 32) && !(e && south)) {
				continue;
			}
			if ((mask & 64) && !(south && w)) {
				continue;
			}
			if ((mask & 128) && !(w && n)) {
				continue;
			}
			result.push_back(mask);
		}
		return result;
	}();
	return masks;
}

[[nodiscard]] int AutotileIndex(
	PaintAutotileFormat format, const std::function<bool(V2_int)>& occupied, V2_int cell
) {
	if (format == PaintAutotileFormat::Blob47) {
		const int mask{ BlobOccupancyMask(occupied, cell) };
		const auto& valid{ ValidBlob47Masks() };
		if (const auto it{ std::ranges::find(valid, mask) }; it != valid.end()) {
			return static_cast<int>(std::distance(valid.begin(), it));
		}
		return 0;
	}
	const int mask{ CardinalOccupancyMask(occupied, cell) };
	if (format == PaintAutotileFormat::Classic15) {
		return mask == 0 ? 0 : std::clamp(mask - 1, 0, 14);
	}
	return std::clamp(mask, 0, 15);
}

void DrawToolIcon(ImDrawList* draw, PaintTool tool, ImVec2 min, ImU32 color, float scale = 1.0f) {
	const float x{ std::floor(min.x) };
	const float y{ std::floor(min.y) };
	auto point = [&](float px, float py) {
		return ImVec2{ x + px * scale, y + py * scale };
	};
	const float line{ std::max(1.0f, 1.6f * scale) };

	switch (tool) {
		case PaintTool::Select:
			draw->AddTriangleFilled(
				point(2.0f, 1.5f), point(2.0f, 13.5f), point(6.3f, 9.7f), color
			);
			draw->AddLine(point(6.0f, 9.2f), point(10.5f, 14.0f), color, line);
			break;

		case PaintTool::Move:
			draw->AddLine(point(8.0f, 2.0f), point(8.0f, 14.0f), color, line);
			draw->AddLine(point(2.0f, 8.0f), point(14.0f, 8.0f), color, line);
			draw->AddTriangleFilled(
				point(8.0f, 0.5f), point(5.4f, 4.0f), point(10.6f, 4.0f), color
			);
			draw->AddTriangleFilled(
				point(8.0f, 15.5f), point(5.4f, 12.0f), point(10.6f, 12.0f), color
			);
			draw->AddTriangleFilled(
				point(0.5f, 8.0f), point(4.0f, 5.4f), point(4.0f, 10.6f), color
			);
			draw->AddTriangleFilled(
				point(15.5f, 8.0f), point(12.0f, 5.4f), point(12.0f, 10.6f), color
			);
			break;

		case PaintTool::Pencil:
			draw->AddLine(point(3.0f, 12.7f), point(11.7f, 4.0f), color, 2.5f * scale);
			draw->AddQuadFilled(
				point(2.0f, 14.0f), point(3.1f, 10.8f), point(5.2f, 12.9f), point(2.0f, 14.8f),
				color
			);
			draw->AddLine(point(10.8f, 3.2f), point(13.0f, 5.4f), color, line);
			break;

		case PaintTool::Brush:
			draw->AddLine(point(11.8f, 2.2f), point(7.0f, 8.6f), color, 2.5f * scale);
			draw->AddBezierCubic(
				point(6.8f, 8.2f), point(7.0f, 11.2f), point(4.5f, 14.2f), point(1.8f, 13.2f),
				color, line
			);
			draw->AddBezierCubic(
				point(1.8f, 13.2f), point(4.1f, 12.4f), point(2.8f, 9.3f), point(6.8f, 8.2f), color,
				line
			);
			break;

		case PaintTool::Line:
			draw->AddLine(point(2.5f, 13.5f), point(13.5f, 2.5f), color, 2.0f * scale);
			draw->AddCircleFilled(point(2.5f, 13.5f), 1.35f * scale, color, 10);
			draw->AddCircleFilled(point(13.5f, 2.5f), 1.35f * scale, color, 10);
			break;

		case PaintTool::Rectangle:
			draw->AddRect(point(2.0f, 3.0f), point(14.0f, 13.0f), color, 0.5f, 0, line);
			break;

		case PaintTool::Fill:
			draw->AddQuad(
				point(4.0f, 3.0f), point(11.5f, 6.5f), point(7.5f, 13.5f), point(1.5f, 10.0f),
				color, line
			);
			draw->AddLine(point(5.2f, 2.0f), point(12.0f, 8.8f), color, line);
			draw->AddCircleFilled(point(12.8f, 12.5f), 1.7f * scale, color, 10);
			break;

		case PaintTool::Erase:
			draw->AddQuadFilled(
				point(4.0f, 3.3f), point(13.3f, 8.0f), point(8.2f, 14.0f), point(1.2f, 10.3f), color
			);
			draw->AddLine(
				point(4.1f, 11.8f), point(10.0f, 6.2f), ImGui::GetColorU32(ImGuiCol_Button), line
			);
			break;

		case PaintTool::Eyedropper:
			draw->AddLine(point(4.0f, 12.5f), point(11.2f, 5.3f), color, 2.5f * scale);
			draw->AddCircle(point(12.1f, 4.1f), 2.4f * scale, color, 12, line);
			draw->AddLine(point(2.0f, 14.0f), point(5.2f, 10.8f), color, line);
			break;
	}
}

[[nodiscard]] bool ContainsInsensitive(std::string_view value, std::string_view query) {
	if (query.empty()) {
		return true;
	}
	std::string a{ value };
	std::string b{ query };
	std::ranges::transform(a, a.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	std::ranges::transform(b, b.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return a.contains(b);
}

[[nodiscard]] std::string Trim(std::string value) {
	auto not_space = [](unsigned char c) {
		return !std::isspace(c);
	};
	auto first{ std::ranges::find_if(value, not_space) };
	auto last{ std::ranges::find_if(value | std::views::reverse, not_space).base() };
	if (first >= last) {
		return {};
	}
	return std::string{ first, last };
}

[[nodiscard]] bool IsImagePath(const std::filesystem::path& path) {
	std::string ext{ path.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif";
}

[[nodiscard]] std::optional<V2_int> FilenameTileDimensions(const std::filesystem::path& path) {
	static const std::regex dimensions{ R"((?:_|-)([0-9]+)x([0-9]+)$)", std::regex::icase };
	std::smatch match;
	const std::string stem{ path.stem().string() };
	if (!std::regex_search(stem, match, dimensions) || match.size() < 3) {
		return std::nullopt;
	}
	try {
		return V2_int{ std::max(1, std::stoi(match[1].str())),
					   std::max(1, std::stoi(match[2].str())) };
	} catch (...) {
		return std::nullopt;
	}
}

[[nodiscard]] std::string SourceGroupName(const std::filesystem::path& path) {
	static const std::regex dimensions{ R"((?:_|-)[0-9]+x[0-9]+$)", std::regex::icase };
	std::string result{ std::regex_replace(path.stem().string(), dimensions, "") };
	return result.empty() ? "Tiles" : result;
}

struct TiledTilesetInfo {
	int tile_width{};
	int tile_height{};
	int margin{};
	int spacing{};
	std::vector<std::string> images{};
};

[[nodiscard]] std::string ReadTextFile(const std::filesystem::path& path) {
	std::ifstream file{ path, std::ios::binary };
	if (!file) {
		return {};
	}
	std::ostringstream stream;
	stream << file.rdbuf();
	return stream.str();
}

[[nodiscard]] int RegexInt(const std::string& text, const std::regex& pattern, int fallback = 0) {
	std::smatch match;
	if (!std::regex_search(text, match, pattern) || match.size() < 2) {
		return fallback;
	}
	try {
		return std::stoi(match[1].str());
	} catch (...) {
		return fallback;
	}
}

[[nodiscard]] TiledTilesetInfo ParseTiledTileset(const std::filesystem::path& path) {
	TiledTilesetInfo info;
	const std::string text{ ReadTextFile(path) };
	if (text.empty()) {
		return info;
	}
	std::string ext{ path.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (ext == ".tsx") {
		info.tile_width =
			RegexInt(text, std::regex{ R"rx(tilewidth\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.tile_height =
			RegexInt(text, std::regex{ R"rx(tileheight\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.margin =
			RegexInt(text, std::regex{ R"rx(margin\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.spacing =
			RegexInt(text, std::regex{ R"rx(spacing\s*=\s*"([0-9]+)")rx", std::regex::icase });
		const std::regex image{ R"rx(<image[^>]*\bsource\s*=\s*"([^"]+)")rx", std::regex::icase };
		for (std::sregex_iterator it{ text.begin(), text.end(), image }, end; it != end; ++it) {
			info.images.push_back((*it)[1].str());
		}
	} else {
		info.tile_width =
			RegexInt(text, std::regex{ R"rx("tilewidth"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.tile_height =
			RegexInt(text, std::regex{ R"rx("tileheight"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.margin =
			RegexInt(text, std::regex{ R"rx("margin"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.spacing =
			RegexInt(text, std::regex{ R"rx("spacing"\s*:\s*([0-9]+))rx", std::regex::icase });
		const std::regex image{ R"rx("image"\s*:\s*"([^"]+)")rx", std::regex::icase };
		for (std::sregex_iterator it{ text.begin(), text.end(), image }, end; it != end; ++it) {
			std::string value{ (*it)[1].str() };
			for (std::size_t pos{}; (pos = value.find("\\/", pos)) != std::string::npos;) {
				value.replace(pos, 2, "/");
			}
			info.images.push_back(std::move(value));
		}
	}
	return info;
}

[[nodiscard]] FileDialog::Options TileImportOptions() {
	return FileDialog::Options{
		.filters = {
			{ .name = "Tiles and Tiled tilesets", .spec = "png,jpg,jpeg,bmp,gif,tsx,tsj,json" },
			{ .name = "Images", .spec = "png,jpg,jpeg,bmp,gif" },
			{ .name = "Tiled tilesets", .spec = "tsx,tsj,json" },
			{ .name = "All files", .spec = "*" },
		},
	};
}

constexpr float kRecipeLabelWidth{ 116.0f };
constexpr float kRecipeControlWidth{ 190.0f };

void BeginRecipeField(const char* label, float control_width = kRecipeControlWidth) {
	const float start_x{ ImGui::GetCursorPosX() };

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();

	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), start_x + kRecipeLabelWidth));
	ImGui::SetNextItemWidth(
		std::min(control_width, std::max(1.0f, ImGui::GetContentRegionAvail().x))
	);
}

bool DrawOriginCombo(const char* id, Origin& origin) {
	struct Entry {
		Origin value;
		const char* label;
	};

	static constexpr std::array entries{
		Entry{ Origin::TopLeft, "Top Left" },
		Entry{ Origin::CenterTop, "Top" },
		Entry{ Origin::TopRight, "Top Right" },
		Entry{ Origin::CenterLeft, "Left" },
		Entry{ Origin::Center, "Center" },
		Entry{ Origin::CenterRight, "Right" },
		Entry{ Origin::BottomLeft, "Bottom Left" },
		Entry{ Origin::CenterBottom, "Bottom" },
		Entry{ Origin::BottomRight, "Bottom Right" },
	};

	const char* selected_label{ "Center" };
	for (const auto& entry : entries) {
		if (entry.value == origin) {
			selected_label = entry.label;
			break;
		}
	}

	bool changed{};
	if (ImGui::BeginCombo(id, selected_label)) {
		for (const auto& entry : entries) {
			const bool selected{ entry.value == origin };
			if (ImGui::Selectable(entry.label, selected)) {
				origin	= entry.value;
				changed = true;
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}
	return changed;
}

struct PaintSquareButtonResult {
	bool pressed{};
	bool hovered{};
	ImVec2 min{};
	float side{};
};

PaintSquareButtonResult DrawPaintSquareButton(
	const char* id, bool selected, std::string_view tooltip
) {
	const auto& style{ ImGui::GetStyle() };
	const float side{ ImGui::GetFrameHeight() };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };

	ImGui::PushItemFlag(ImGuiItemFlags_NoNav, true);
	ImGui::InvisibleButton(id, { side, side });
	ImGui::PopItemFlag();

	const bool hovered{ ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) };
	const bool active{ ImGui::IsItemActive() };
	const bool pressed{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };

	const ImVec4 background{ selected  ? style.Colors[ImGuiCol_ButtonActive]
							 : active  ? style.Colors[ImGuiCol_ButtonActive]
							 : hovered ? style.Colors[ImGuiCol_ButtonHovered]
									   : style.Colors[ImGuiCol_Button] };

	auto* draw{ ImGui::GetWindowDrawList() };
	draw->AddRectFilled(
		p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(background), style.FrameRounding
	);
	if (selected) {
		draw->AddRect(
			p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_Text),
			style.FrameRounding, 0, 1.0f
		);
	}

	if (hovered && !tooltip.empty()) {
		ImGui::SetTooltip("%.*s", static_cast<int>(tooltip.size()), tooltip.data());
	}

	return PaintSquareButtonResult{
		.pressed = pressed,
		.hovered = hovered,
		.min	 = p0,
		.side	 = side,
	};
}

bool DrawLockToggleButton(const char* id, bool& locked) {
	const auto button{ DrawPaintSquareButton(
		id, locked, locked ? "Unlock grid aspect ratio" : "Lock grid aspect ratio"
	) };

	auto* draw{ ImGui::GetWindowDrawList() };
	const ImU32 color{ ImGui::GetColorU32(ImGuiCol_Text) };
	const float s{ button.side };
	const float cx{ button.min.x + s * 0.5f };
	const float top{ button.min.y + s * 0.26f };
	const float body_top{ button.min.y + s * 0.48f };
	const float body_bottom{ button.min.y + s * 0.76f };
	const float half_w{ s * 0.18f };

	draw->AddRect({ cx - half_w, body_top }, { cx + half_w, body_bottom }, color, 1.5f, 0, 1.4f);
	draw->AddBezierCubic(
		{ cx - half_w * 0.72f, body_top }, { cx - half_w * 0.72f, top },
		{ cx + half_w * 0.72f, top }, { cx + half_w * 0.72f, body_top }, color, 1.4f
	);

	if (button.pressed) {
		locked = !locked;
	}
	return button.pressed;
}

Color GridArrayToColor(const std::array<float, 4>& value) {
	auto byte = [](float channel) {
		return static_cast<std::uint8_t>(std::lround(std::clamp(channel, 0.0f, 1.0f) * 255.0f));
	};
	return Color{
		byte(value[0]),
		byte(value[1]),
		byte(value[2]),
		byte(value[3]),
	};
}

void ColorToGridArray(Color value, std::array<float, 4>& output) {
	output = {
		static_cast<float>(value.r) / 255.0f,
		static_cast<float>(value.g) / 255.0f,
		static_cast<float>(value.b) / 255.0f,
		static_cast<float>(value.a) / 255.0f,
	};
}

void DrawTileThumbnail(EditorContext& ctx, const PaintTileSource& source, float side) {
	const ImVec2 min{ ImGui::GetCursorScreenPos() };
	ImGui::Dummy({ side, side });
	const ImVec2 max{ min.x + side, min.y + side };

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	const auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
		return candidate.kind == AssetKind::Texture &&
			   candidate.key == static_cast<const AssetKey&>(source.texture);
	}) };

	auto* draw{ ImGui::GetWindowDrawList() };
	draw->AddRectFilled(
		min, max, ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding
	);

	if (record != records.end() && record->preview.has_value()) {
		const auto& uv{ source.texture_coordinates };
		draw->AddImageQuad(
			static_cast<ImTextureID>(record->preview->texture), min, { max.x, min.y }, max,
			{ min.x, max.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]), ToImGui(uv[3])
		);
	}

	draw->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding);
}

void DrawToolButton(PaintTool tool, PaintTool& selected) {
	std::string tooltip{ ToolName(tool) };
	tooltip += " (";
	tooltip += ToolShortcut(tool);
	tooltip += ")";

	const auto button{ DrawPaintSquareButton(
		("##PaintTool" + std::to_string(static_cast<int>(tool))).c_str(), tool == selected, tooltip
	) };

	const float icon_scale{ std::clamp((button.side - 8.0f) / 16.0f, 0.65f, 0.95f) };
	const float extent{ 16.0f * icon_scale };
	DrawToolIcon(
		ImGui::GetWindowDrawList(), tool,
		{
			button.min.x + (button.side - extent) * 0.5f,
			button.min.y + (button.side - extent) * 0.5f,
		},
		ImGui::GetColorU32(ImGuiCol_Text), icon_scale
	);

	if (button.pressed) {
		selected = tool;
	}
}

} // namespace

void PaintEditor::SetTool(PaintTool tool) {
	if (tool_ == tool) {
		return;
	}
	tool_ = tool;
	stroke_ = {};
	move_ = {};
	selection_drag_start_ = {};
	selection_drag_active_ = false;
}

SceneLayerId PaintEditor::GetActiveLayer(const Scene& scene) const {
	if (active_layer_ && scene.GetLayers().Find(active_layer_)) {
		return active_layer_;
	}
	return scene.GetLayers().GetDefaultEntityLayer();
}

void PaintEditor::SetActiveLayer(Scene& scene, SceneLayerId layer) {
	if (!scene.GetLayers().Find(layer)) {
		return;
	}
	active_layer_ = layer;

	const SceneLayer* definition{ scene.GetLayers().Find(layer) };
	if (definition && definition->kind == SceneLayerKind::Tile) {
		Tilemap target{ ResolveTargetTilemap(scene) };
		if (!target) {
			for (Entity entity : scene.GetLayers().GetRootEntities(scene, layer)) {
				if (IsTilemap(entity)) {
					target_tilemap_ = entity.Get<UUID>();
					break;
				}
			}
		}
	} else {
		selected_tile_cells_.clear();
		selected_tilemap_.reset();
	}
}

void PaintEditor::SetTargetTilemap(std::optional<UUID> uuid) {
	target_tilemap_ = uuid;
	selected_tilemap_ = uuid;
	selected_tile_cells_.clear();
	selected_entities_.clear();
	selected_generator_.reset();
	selection_drag_start_ = {};
	selection_drag_active_ = false;
}

SceneLayer* PaintEditor::ResolveActiveLayer(Scene& scene) {
	const SceneLayerId id{ GetActiveLayer(scene) };
	active_layer_ = id;
	return scene.GetLayers().Find(id);
}

const SceneLayer* PaintEditor::ResolveActiveLayer(const Scene& scene) const {
	return scene.GetLayers().Find(GetActiveLayer(scene));
}

Tilemap PaintEditor::ResolveTargetTilemap(Scene& scene) {
	const SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->kind != SceneLayerKind::Tile) {
		return {};
	}

	if (target_tilemap_.has_value()) {
		Entity entity{ scene.GetEntity(*target_tilemap_) };
		if (entity && IsTilemap(entity) && scene.GetLayers().GetLayerId(entity) == layer->id) {
			return Tilemap{ entity };
		}
	}

	for (Entity entity : scene.GetLayers().GetRootEntities(scene, layer->id)) {
		if (IsTilemap(entity)) {
			target_tilemap_ = entity.Get<UUID>();
			return Tilemap{ entity };
		}
	}
	return {};
}

Tilemap PaintEditor::ResolveTargetTilemap(const Scene& scene) const {
	const SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->kind != SceneLayerKind::Tile || !target_tilemap_.has_value()) {
		return {};
	}
	Entity entity{ scene.GetEntity(*target_tilemap_) };
	if (!entity || !IsTilemap(entity) || scene.GetLayers().GetLayerId(entity) != layer->id) {
		return {};
	}
	return Tilemap{ entity };
}

void PaintEditor::SyncHierarchySelection(EditorContext& ctx, Scene& scene) {
	Entity selected{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	if (!selected || &selected.GetScene() != &scene || IsTilemap(selected)) {
		return;
	}

	const UUID uuid{ selected.Get<UUID>() };
	const bool already_selected{
		(IsPaintGenerator(selected) && selected_generator_ == uuid) ||
		(!IsPaintGenerator(selected) && std::ranges::contains(selected_entities_, uuid))
	};
	if (already_selected) {
		return;
	}

	selected_entities_.clear();
	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();
	selection_drag_start_ = {};
	selection_drag_active_ = false;

	if (IsPaintGenerator(selected)) {
		selected_generator_ = uuid;
	} else if (!IsProtectedSceneEntity(scene, selected)) {
		selected_entities_.push_back(uuid);
	}
}

void PaintEditor::ValidateSceneState(Scene& scene) {
	if (!scene.GetLayers().Find(active_layer_)) {
		active_layer_ = scene.GetLayers().GetDefaultEntityLayer();
	}

	if (target_tilemap_.has_value()) {
		Entity target{ scene.GetEntity(*target_tilemap_) };
		if (!target || !IsTilemap(target)) {
			target_tilemap_.reset();
		}
	}

	std::erase_if(selected_entities_, [&](UUID uuid) {
		Entity entity{ scene.GetEntity(uuid) };
		return !entity || IsTilemap(entity) || IsPaintGenerator(entity);
	});

	if (selected_generator_.has_value()) {
		Entity generator{ scene.GetEntity(*selected_generator_) };
		if (!generator || !IsPaintGenerator(generator)) {
			selected_generator_.reset();
		}
	}

	if (selected_tilemap_.has_value()) {
		Entity selected{ scene.GetEntity(*selected_tilemap_) };
		if (!selected || !IsTilemap(selected)) {
			selected_tilemap_.reset();
			selected_tile_cells_.clear();
		} else {
			Tilemap map{ selected };
			std::erase_if(selected_tile_cells_, [&](V2_int cell) {
				return map.FindTile(cell) == nullptr;
			});
		}
	}
}

void PaintEditor::ClearSelection() {
	selected_entities_.clear();
	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();
	selection_drag_start_ = {};
	selection_drag_active_ = false;
}

bool PaintEditor::HasSelection() const {
	return selected_generator_.has_value() || !selected_entities_.empty() ||
		!selected_tile_cells_.empty();
}

void PaintEditor::DrawViewportToolButtons(EditorContext& ctx) {
	EnsureLocalState(ctx);

	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	if (!scene) {
		return;
	}
	ValidateSceneState(*scene);
	SyncHierarchySelection(ctx, *scene);

	SceneLayer* layer{ ResolveActiveLayer(*scene) };
	if (!layer) {
		return;
	}

	static constexpr std::array tools{
		PaintTool::Select, PaintTool::Move,	 PaintTool::Pencil,
		PaintTool::Brush,  PaintTool::Line,	 PaintTool::Rectangle,
		PaintTool::Fill,   PaintTool::Erase, PaintTool::Eyedropper,
	};

	for (std::size_t i{}; i < tools.size(); ++i) {
		if (i != 0) {
			ImGui::SameLine();
		}

		ImGui::BeginDisabled(layer->locked);
		PaintTool next{ tool_ };
		DrawToolButton(tools[i], next);
		if (next != tool_) {
			SetTool(next);
		}
		ImGui::EndDisabled();
	}

	ImGui::SameLine();
	ImGui::PushID("PaintGridToolbar");

	const auto grid_button{ DrawPaintSquareButton("##Grid", grid_visible_, "Grid settings") };

	const ImU32 foreground{ ImGui::GetColorU32(ImGuiCol_Text) };
	const float padding{ std::max(4.0f, grid_button.side * 0.25f) };
	const float x0{ grid_button.min.x + padding };
	const float x1{ grid_button.min.x + grid_button.side - padding };
	const float y0{ grid_button.min.y + padding };
	const float y1{ grid_button.min.y + grid_button.side - padding };

	auto* draw{ ImGui::GetWindowDrawList() };
	for (int i{ 1 }; i <= 2; ++i) {
		const float t{ static_cast<float>(i) / 3.0f };
		draw->AddLine({ x0 + (x1 - x0) * t, y0 }, { x0 + (x1 - x0) * t, y1 }, foreground, 1.0f);
		draw->AddLine({ x0, y0 + (y1 - y0) * t }, { x1, y0 + (y1 - y0) * t }, foreground, 1.0f);
	}

	if (grid_button.pressed) {
		ImGui::OpenPopup("PaintGridSettingsPopup");
	}

	if (ImGui::BeginPopup("PaintGridSettingsPopup")) {
		ImGui::SeparatorText("Grid");

		BeginRecipeField("Visible", 190.0f);
		ImGui::Checkbox("##GridVisible", &grid_visible_);

		if (layer->kind == SceneLayerKind::Entity) {
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const float lock_width{ ImGui::GetFrameHeight() };
			const float value_width{ 190.0f };
			const float pair_width{ std::max(1.0f, value_width - lock_width - spacing) };
			const float field_width{ std::max(1.0f, (pair_width - spacing) * 0.5f) };

			const float start_x{ ImGui::GetCursorPosX() };
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Cell Size");
			ImGui::SameLine();
			ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), start_x + kRecipeLabelWidth));

			float width{ entity_grid_size_.x };
			float height{ entity_grid_size_.y };

			ImGui::SetNextItemWidth(field_width);
			const bool width_changed{
				ImGui::DragFloat("##GridWidth", &width, 0.25f, 1.0f, 4096.0f, "W: %.0f")
			};

			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			const bool height_changed{
				ImGui::DragFloat("##GridHeight", &height, 0.25f, 1.0f, 4096.0f, "H: %.0f")
			};

			ImGui::SameLine(0.0f, spacing);
			if (DrawLockToggleButton("##GridAspectLock", grid_aspect_locked_) &&
				grid_aspect_locked_) {
				grid_locked_aspect_ = entity_grid_size_.x / std::max(1.0f, entity_grid_size_.y);
			}

			if (width_changed || height_changed) {
				if (grid_aspect_locked_) {
					if (width_changed && !height_changed) {
						height = width / std::max(0.001f, grid_locked_aspect_);
					} else {
						width = height * grid_locked_aspect_;
					}
				}

				entity_grid_size_ = {
					std::max(1.0f, width),
					std::max(1.0f, height),
				};
			}

			float offset[2]{
				entity_grid_offset_.x,
				entity_grid_offset_.y,
			};
			BeginRecipeField("Offset", 190.0f);
			if (ImGui::DragFloat2("##PaintGridOffset", offset, 0.25f)) {
				entity_grid_offset_ = {
					offset[0],
					offset[1],
				};
			}
		}

		ImGui::SeparatorText("Minor Lines");
		{
			Color color{ GridArrayToColor(grid_minor_color_) };
			BeginRecipeField("Color", 190.0f);
			if (DrawColorEdit(ctx, "##GridMinorColor", color)) {
				ColorToGridArray(color, grid_minor_color_);
			}
		}
		BeginRecipeField("Thickness", 190.0f);
		ImGui::DragFloat(
			"##GridMinorThickness", &grid_minor_thickness_, 0.05f, 0.25f, 8.0f, "%.2f"
		);

		ImGui::SeparatorText("Major Lines");
		BeginRecipeField("Every", 190.0f);
		ImGui::DragInt("##GridMajorEvery", &grid_major_every_, 0.2f, 1, 128);
		grid_major_every_ = std::max(1, grid_major_every_);

		{
			Color color{ GridArrayToColor(grid_major_color_) };
			BeginRecipeField("Color", 190.0f);
			if (DrawColorEdit(ctx, "##GridMajorColor", color)) {
				ColorToGridArray(color, grid_major_color_);
			}
		}
		BeginRecipeField("Thickness", 190.0f);
		ImGui::DragFloat(
			"##GridMajorThickness", &grid_major_thickness_, 0.05f, 0.25f, 8.0f, "%.2f"
		);

		ImGui::EndPopup();
	}

	ImGui::PopID();
	StoreLocalState(ctx);
}

bool PaintEditor::DrawViewportOptionsToolbar(EditorContext& ctx) {
	EnsureLocalState(ctx);
	EnsureProjectLibrary(ctx);

	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	if (!scene) {
		return false;
	}
	ValidateSceneState(*scene);
	SyncHierarchySelection(ctx, *scene);

	SceneLayer* layer{ ResolveActiveLayer(*scene) };
	if (!layer) {
		return false;
	}

	DrawBrushSettingsToolbar(ctx, *scene, *layer);
	StoreLocalState(ctx);
	return true;
}

void PaintEditor::DrawBrushSettingsToolbar(EditorContext& ctx, Scene& scene, SceneLayer& layer) {
	ImGui::PushID("PaintToolOptions");

	auto same = [] {
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemSpacing.x);
	};

	auto item_at = [](const char* items, int index) {
		const char* item{ items };
		for (int i{}; i < index && *item; ++i) {
			item += std::strlen(item) + 1;
		}
		return item;
	};

	auto compact_width = [](std::string_view text, float extra = 0.0f) {
		return std::ceil(
			ImGui::CalcTextSize(text.data(), text.data() + text.size()).x +
			ImGui::GetStyle().FramePadding.x * 2.0f + extra
		);
	};

	auto combo_width = [&](const char* label, const char* items, int count) {
		float width{};
		for (int i{}; i < count; ++i) {
			std::string preview{ label };
			if (!preview.empty()) {
				preview += ": ";
			}
			preview += item_at(items, i);
			width = std::max(
				width,
				compact_width(preview, ImGui::GetFrameHeight() + 2.0f)
			);
		}
		return width;
	};

	auto combo = [&](const char* id, const char* label, int& value, const char* items, int count) {
		std::string preview{ label };
		if (!preview.empty()) {
			preview += ": ";
		}
		preview += item_at(items, value);

		ImGui::SetNextItemWidth(combo_width(label, items, count));

		bool changed{};
		if (ImGui::BeginCombo(id, preview.c_str())) {
			for (int i{}; i < count; ++i) {
				const bool selected{ i == value };
				if (ImGui::Selectable(item_at(items, i), selected)) {
					value = i;
					changed = true;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return changed;
	};

	bool first_control{ true };
	auto next_control = [&] {
		if (!first_control) {
			same();
		}
		first_control = false;
	};

	const float control_height{ ImGui::GetFrameHeight() };

	if (tool_ == PaintTool::Select) {
		int mode{ static_cast<int>(select_mode_) };
		next_control();
		if (combo("##SelectMode", "Mode", mode, "Click + Marquee\0Selection Brush\0", 2)) {
			select_mode_ = static_cast<PaintSelectMode>(mode);
			selection_drag_start_ = {};
			selection_drag_active_ = false;
		}

		if (select_mode_ == PaintSelectMode::Brush) {
			next_control();
			ImGui::SetNextItemWidth(compact_width("Diameter: 128"));
			ImGui::DragInt(
				"##SelectionDiameter", &selection_diameter_, 0.2f, 1, 128, "Diameter: %d"
			);
			selection_diameter_ = std::clamp(selection_diameter_, 1, 128);

			next_control();
			int shape{ static_cast<int>(selection_brush_shape_) };
			if (combo("##SelectionShape", "Shape", shape, "Circle\0Square\0", 2)) {
				selection_brush_shape_ = static_cast<PaintBrushShape>(shape);
			}
		}

		next_control();
		const bool has_selection{
			HasSelection() || static_cast<bool>(ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity())
		};

		ImGui::BeginDisabled(!has_selection);
		if (ImGui::Button("Deselect", { 0.0f, control_height })) {
			ClearSelection();
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		}
		ImGui::EndDisabled();

		ImGui::PopID();
		return;
	}

	if (tool_ == PaintTool::Move) {
		int snap{ static_cast<int>(move_snap_) };
		next_control();
		if (combo("##MoveMode", "Move", snap, "Grid\0Free\0", 2)) {
			move_snap_ = static_cast<PaintMoveSnapMode>(snap);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Ctrl/Cmd temporarily inverts Grid/Free movement while dragging.");
		}

		next_control();
		const bool has_selection{ HasSelection() };
		ImGui::BeginDisabled(!has_selection);
		if (ImGui::Button("Snap to Grid", { 0.0f, control_height })) {
			SnapSelectionToGrid(ctx, scene);
		}
		ImGui::EndDisabled();
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip(
				"Snap the selected entity, generator, or tiles to the nearest grid coordinates."
			);
		}

		ImGui::PopID();
		return;
	}

	if (tool_ == PaintTool::Eyedropper) {
		// Eyedropper intentionally has no options, but the options row must keep the
		// same height as every other paint tool so switching tools does not resize
		// the viewport.
		ImGui::Dummy({ 0.0f, control_height });
		ImGui::PopID();
		return;
	}

	const bool tile_layer{ layer.kind == SceneLayerKind::Tile };
	const bool entity_layer{ layer.kind == SceneLayerKind::Entity };
	const bool emits_content{
		tool_ == PaintTool::Pencil || tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
		tool_ == PaintTool::Rectangle || tool_ == PaintTool::Fill
	};

	if (tool_ == PaintTool::Erase) {
		int erase_mode{ recipe_.operation == PaintBrushOperation::ExclusionMask ? 1 : 0 };
		if (!tile_layer && erase_mode != 0) {
			erase_mode = 0;
			recipe_.operation = PaintBrushOperation::Paint;
		}

		next_control();
		if (combo(
				"##EraseOperation", "Mode", erase_mode,
				tile_layer ? "Erase Tiles\0Erase Mask\0" : "Erase Entities\0",
				tile_layer ? 2 : 1
			)) {
			recipe_.operation =
				erase_mode == 1 ? PaintBrushOperation::ExclusionMask : PaintBrushOperation::Paint;
		}
	} else if (emits_content) {
		int operation{ static_cast<int>(recipe_.operation) };
		const bool allow_mask{ tile_layer && tool_ != PaintTool::Fill };
		const char* operations{ allow_mask ? "Paint\0Replace\0Mask\0" : "Paint\0Replace\0" };
		const int count{ allow_mask ? 3 : 2 };

		if (operation >= count) {
			operation = 0;
			recipe_.operation = PaintBrushOperation::Paint;
		}

		next_control();
		if (combo("##PaintOperation", "Mode", operation, operations, count)) {
			recipe_.operation = static_cast<PaintBrushOperation>(operation);
		}
		if (ImGui::IsItemHovered() && allow_mask) {
			ImGui::SetTooltip(
				"Paint emits content, Replace overwrites eligible content, and Mask authors "
				"the Tilemap exclusion mask."
			);
		}
	}

	if (tool_ == PaintTool::Brush || tool_ == PaintTool::Erase) {
		next_control();
		if (ImGui::Button("-##PaintDiameter", { control_height, control_height })) {
			brush_diameter_ = std::max(1, brush_diameter_ - 1);
		}

		same();
		ImGui::SetNextItemWidth(compact_width("Diameter: 128"));
		ImGui::DragInt("##PaintDiameterValue", &brush_diameter_, 0.2f, 1, 128, "Diameter: %d");
		brush_diameter_ = std::clamp(brush_diameter_, 1, 128);

		same();
		if (ImGui::Button("+##PaintDiameter", { control_height, control_height })) {
			brush_diameter_ = std::min(128, brush_diameter_ + 1);
		}

		next_control();
		int shape{ static_cast<int>(brush_shape_) };
		if (combo("##BrushShape", "Shape", shape, "Circle\0Square\0", 2)) {
			brush_shape_ = static_cast<PaintBrushShape>(shape);
		}
	}

	if (tool_ == PaintTool::Line) {
		next_control();
		ImGui::SetNextItemWidth(compact_width("Thickness: 32"));
		ImGui::DragInt("##LineThickness", &line_thickness_, 0.2f, 1, 32, "Thickness: %d");
		line_thickness_ = std::max(1, line_thickness_);

		next_control();
		ImGui::SetNextItemWidth(compact_width("Spacing: 32"));
		ImGui::DragInt("##LineSpacing", &line_spacing_, 0.2f, 1, 32, "Spacing: %d");
		line_spacing_ = std::max(1, line_spacing_);
	}

	if (tool_ == PaintTool::Rectangle) {
		next_control();
		int mode{ static_cast<int>(area_mode_) };
		if (combo("##AreaMode", "Area", mode, "Fill\0Outline\0Corners\0Random Fill\0", 4)) {
			area_mode_ = static_cast<PaintAreaMode>(mode);
		}

		if (area_mode_ == PaintAreaMode::Outline || area_mode_ == PaintAreaMode::Corners) {
			next_control();
			ImGui::SetNextItemWidth(compact_width("Thickness: 32"));
			ImGui::DragInt("##AreaThickness", &area_thickness_, 0.2f, 1, 32, "Thickness: %d");
			area_thickness_ = std::max(1, area_thickness_);
		} else if (area_mode_ == PaintAreaMode::RandomFill) {
			next_control();
			ImGui::SetNextItemWidth(compact_width("Density: 1.00"));
			ImGui::SliderFloat("##AreaDensity", &recipe_.density, 0.0f, 1.0f, "Density: %.2f");
		}
	}

	const bool recipe_applies{
		emits_content && recipe_.operation != PaintBrushOperation::ExclusionMask
	};

	if (recipe_applies) {
		if (entity_layer && recipe_.source_kind == PaintSourceKind::Autotile) {
			recipe_.source_kind = PaintSourceKind::Single;
		}

		if (entity_layer) {
			next_control();
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted("Linked");
			const bool linked_label_hovered{ ImGui::IsItemHovered() };
			ImGui::SameLine(0.0f, 4.0f);
			ImGui::Checkbox("##PaintLinkedPrefabInstances", &recipe_.link_prefab_instances);
			if (linked_label_hovered || ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Keep painted prefab instances linked to their prefab asset in the editor. "
					"Disable to bake them into ordinary entities immediately."
				);
			}
		}

		const bool finite_generator_tool{
			tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
			tool_ == PaintTool::Rectangle
		};
		if (finite_generator_tool && recipe_.operation == PaintBrushOperation::Paint) {
			int result{ static_cast<int>(recipe_.commit_mode) };
			next_control();
			if (combo("##PaintResult", "Result", result, "Bake\0Keep Generator\0", 2)) {
				recipe_.commit_mode = static_cast<PaintCommitMode>(result);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Bake creates ordinary editable content. Keep Generator stores the authored "
					"geometry and a captured recipe."
				);
			}
		}

		next_control();
		ImGui::SetNextItemWidth(compact_width("More", ImGui::GetFrameHeight() + 2.0f));
		ImGui::SetNextWindowSizeConstraints(
			ImVec2{ 330.0f, 0.0f },
			ImVec2{ 460.0f, 520.0f }
		);
		if (ImGui::BeginCombo("##PaintMore", "More")) {
			if (tool_ == PaintTool::Line && entity_layer) {
				ImGui::Checkbox("Align entities to line", &line_align_rotation_);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Rotate newly placed prefabs to match the authored line direction."
					);
				}
			}

			if (tile_layer && recipe_.source_kind != PaintSourceKind::Autotile) {
				Tilemap target{ ResolveTargetTilemap(scene) };
				const V2_float cell_size{
					target ? target.GetData().cell_size : ActiveGridSize(scene)
				};

				bool source_size_differs{};
				auto test_source = [&](const PaintTileSource& source) {
					if (!source) {
						return;
					}

					source_size_differs |=
						std::abs(static_cast<float>(source.pixel_size.x) - cell_size.x) > 0.01f ||
						std::abs(static_cast<float>(source.pixel_size.y) - cell_size.y) > 0.01f;
				};

				if (recipe_.source_kind == PaintSourceKind::Single) {
					test_source(tile_source_);
				} else if (recipe_.source_kind == PaintSourceKind::Checkerboard) {
					test_source(tile_source_);
					if (recipe_.checker_tile) {
						test_source(*recipe_.checker_tile);
					}
				} else if (recipe_.source_kind == PaintSourceKind::WeightedSet) {
					if (const auto* set{ FindWeightedTileSet(recipe_.weighted_tile_set_name) }) {
						for (const auto& entry : set->entries) {
							test_source(entry.source);
						}
					}
				} else if (recipe_.source_kind == PaintSourceKind::Noise) {
					for (const auto& region : recipe_.noise.thresholds) {
						if (region.source_kind == PaintSourceKind::Single && region.tile) {
							test_source(*region.tile);
						}

						if (region.source_kind == PaintSourceKind::WeightedSet) {
							if (const auto* set{
									FindWeightedTileSet(region.weighted_tile_set_name) }) {
								for (const auto& entry : set->entries) {
									test_source(entry.source);
								}
							}
						}
					}
				}

				if (source_size_differs) {
					int placement{ static_cast<int>(recipe_.tile_placement) };
					BeginRecipeField("Placement", 190.0f);
					if (ImGui::Combo("##PaintTilePlacement", &placement, "Grid\0Tile\0")) {
						recipe_.tile_placement =
							static_cast<PaintTilePlacementMode>(placement);
					}

					BeginRecipeField("Origin", 190.0f);
					DrawOriginCombo("##PaintTileOrigin", recipe_.tile_origin);
				} else {
					recipe_.tile_placement = PaintTilePlacementMode::Tile;
				}

				if (target && !target.GetData().exclusion_mask.empty()) {
					ImGui::Checkbox("Avoid exclusion mask", &recipe_.avoid_exclusion_mask);
					if (ImGui::IsItemHovered()) {
						ImGui::SetTooltip(
							"Skip ordinary paint and generator output on excluded Tilemap cells."
						);
					}
				}
			}

			if (entity_layer) {
				bool source_matches_grid{ false };
				if (prefab_source_) {
					auto& assets{ ctx.editor.GetAssetManager() };
					if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(prefab_source_)) {
						if (const auto catalog{
								assets.GetCatalogAsset(prefab_source_, AssetKind::Prefab) }) {
							assets.Load(prefab_source_, catalog->source_path);
						}
					}

					if (::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(prefab_source_)) {
						const auto prefab{
							::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_source_)
						};
						const json root_json{ prefab.get().root };
						bool explicit_size{};
						if (root_json.contains("components") &&
							root_json["components"].is_object()) {
							for (auto it{ root_json["components"].begin() };
								 it != root_json["components"].end(); ++it) {
								const std::string key{ ToLower(it.key()) };
								if (key.contains("size") || key.contains("rect") ||
									key.contains("sprite") || key.contains("text") ||
									key.contains("shape")) {
									explicit_size = true;
									break;
								}
							}
						}
						source_matches_grid = !explicit_size;
					}
				}

				if (!source_matches_grid) {
					BeginRecipeField("Origin", 190.0f);
					DrawOriginCombo("##PaintEntityOrigin", recipe_.entity_origin);
				} else {
					recipe_.entity_origin = Origin::Center;
				}

				ImGui::SeparatorText("Transform");
				ImGui::Checkbox("Random Rotation", &recipe_.random_rotation);
				if (recipe_.random_rotation) {
					ImGui::SetNextItemWidth(260.0f);
					ImGui::DragFloatRange2(
						"Rotation##PaintRotationRange",
						&recipe_.rotation_min,
						&recipe_.rotation_max,
						0.5f,
						-3600.0f,
						3600.0f,
						"%.0f",
						"%.0f"
					);
				}

				ImGui::Checkbox("Random Scale", &recipe_.random_scale);
				if (recipe_.random_scale) {
					ImGui::SetNextItemWidth(260.0f);
					ImGui::DragFloatRange2(
						"Scale##PaintScaleRange",
						&recipe_.scale_min,
						&recipe_.scale_max,
						0.01f,
						0.01f,
						8.0f,
						"%.2f",
						"%.2f"
					);
				}

				if (recipe_.coverage != PaintCoverageMode::Solid) {
					ImGui::SetNextItemWidth(220.0f);
					ImGui::DragFloat(
						"Minimum Spacing##PaintMinimumSpacing",
						&recipe_.min_spacing,
						0.25f,
						0.0f,
						4096.0f,
						"%.1f"
					);
				}
			}


			ImGui::EndCombo();
		}
	}

	ImGui::PopID();
}


void PaintEditor::DrawRecipePanel(EditorContext& ctx) {
	EnsureLocalState(ctx);
	EnsureProjectLibrary(ctx);

	if (!ImGui::Begin("Paint Recipe")) {
		ImGui::End();
		return;
	}

	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	if (!scene) {
		ImGui::TextDisabled("Select a scene to edit its paint recipe.");
		ImGui::End();
		return;
	}
	ValidateSceneState(*scene);

	SceneLayer* layer{ ResolveActiveLayer(*scene) };
	if (!layer) {
		ImGui::TextDisabled("Select a Tile or Entity layer to edit its paint recipe.");
		ImGui::End();
		return;
	}

	ImGui::TextDisabled(
		"%s  |  %s layer",
		layer->name.c_str(),
		layer->kind == SceneLayerKind::Tile ? "Tile" : "Entity"
	);
	if (layer->locked) {
		ImGui::SameLine();
		ImGui::TextDisabled("(locked)");
	}

	ImGui::SeparatorText("Source");

	int source_kind{ static_cast<int>(recipe_.source_kind) };
	BeginRecipeField("Source");
	if (layer->kind == SceneLayerKind::Tile) {
		if (ImGui::Combo(
				"##PaintRecipeSource",
				&source_kind,
				"Single\0Weighted Set\0Checkerboard\0Autotile / Terrain\0Noise\0"
			)) {
			recipe_.source_kind = static_cast<PaintSourceKind>(source_kind);
		}
	} else {
		if (recipe_.source_kind == PaintSourceKind::Autotile) {
			recipe_.source_kind = PaintSourceKind::Single;
		}

		int entity_source{
			recipe_.source_kind == PaintSourceKind::Noise
				? 3
				: static_cast<int>(recipe_.source_kind)
		};
		if (entity_source > 3) {
			entity_source = 0;
		}

		if (ImGui::Combo(
				"##PaintRecipeSource",
				&entity_source,
				"Single\0Weighted Set\0Checkerboard\0Noise\0"
			)) {
			recipe_.source_kind = entity_source == 3
				? PaintSourceKind::Noise
				: static_cast<PaintSourceKind>(entity_source);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Choose what the paint tools emit. Source selection is independent from the "
			"Tiles and Prefabs asset tabs."
		);
	}

	if (recipe_.source_kind == PaintSourceKind::Single) {
		if (layer->kind == SceneLayerKind::Tile) {
			DrawTileSourceBrowser(ctx, tile_source_);
		} else {
			DrawPrefabSourceBrowser(ctx, prefab_source_);
		}
	} else if (recipe_.source_kind == PaintSourceKind::Noise) {
		DrawNoiseRecipe(ctx, *scene, *layer);
	} else {
		DrawExtendedSourceRecipe(ctx, *scene, *layer);
	}

	ImGui::SeparatorText("Coverage");

	int coverage{ static_cast<int>(recipe_.coverage) };
	BeginRecipeField("Coverage");
	if (ImGui::Combo(
			"##PaintRecipeCoverage",
			&coverage,
			"Solid\0Random Density\0Radial Falloff\0"
		)) {
		recipe_.coverage = static_cast<PaintCoverageMode>(coverage);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Coverage decides which otherwise-eligible raster cells emit the selected source."
		);
	}

	if (recipe_.coverage == PaintCoverageMode::RandomDensity) {
		BeginRecipeField("Density");
		ImGui::SliderFloat(
			"##PaintRecipeDensity",
			&recipe_.density,
			0.0f,
			1.0f,
			"%.2f"
		);
	} else if (recipe_.coverage == PaintCoverageMode::RadialFalloff) {
		BeginRecipeField("Center Density");
		ImGui::SliderFloat(
			"##PaintRecipeCenterDensity",
			&recipe_.density,
			0.0f,
			1.0f,
			"%.2f"
		);

		BeginRecipeField("Inner");
		ImGui::SliderFloat(
			"##PaintRecipeRadialInner",
			&recipe_.radial_inner,
			0.0f,
			0.95f,
			"%.2f"
		);

		BeginRecipeField("Outer");
		ImGui::SliderFloat(
			"##PaintRecipeRadialOuter",
			&recipe_.radial_outer,
			0.05f,
			1.0f,
			"%.2f"
		);
		recipe_.radial_outer =
			std::max(recipe_.radial_outer, recipe_.radial_inner + 0.01f);
	}

	ImGui::SeparatorText("Procedural");

	ImGui::BeginDisabled(layer->locked);
	if (ImGui::Button("Create Infinite Generator")) {
		CreateInfiniteGenerator(ctx, *scene);
	}
	ImGui::EndDisabled();
	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip(
			"Create a persistent infinite generator using the current source and coverage."
		);
	}

	StoreLocalState(ctx);
	ImGui::End();
}

void PaintEditor::DrawNoiseThresholdGradient(EditorContext&, SceneLayer& layer) {
	auto& field{ recipe_.noise };
	if (field.thresholds.empty()) {
		PaintNoiseThreshold initial;
		initial.minimum = 0.0f;
		initial.maximum = 1.0f;
		initial.enabled = true;
		if (layer.kind == SceneLayerKind::Tile && tile_source_) {
			initial.tile   = tile_source_;
			initial.origin = recipe_.tile_origin;
		} else if (layer.kind == SceneLayerKind::Entity && prefab_source_) {
			initial.prefab = prefab_source_;
			initial.origin = recipe_.entity_origin;
		}
		field.thresholds.push_back(std::move(initial));
	}

	auto normalize = [&] {
		std::ranges::sort(field.thresholds, {}, [](const PaintNoiseThreshold& region) {
			return region.minimum;
		});
		field.thresholds.front().minimum = 0.0f;
		for (std::size_t i{}; i < field.thresholds.size(); ++i) {
			auto& region{ field.thresholds[i] };
			region.minimum = std::clamp(region.minimum, 0.0f, 1.0f);
			region.maximum = std::clamp(region.maximum, region.minimum, 1.0f);
			if (i > 0) {
				region.minimum = field.thresholds[i - 1].maximum;
			}
		}
		field.thresholds.back().maximum = 1.0f;
	};
	auto split = [&](float value) {
		value = std::clamp(value, 0.01f, 0.99f);
		normalize();
		for (std::size_t i{}; i < field.thresholds.size(); ++i) {
			auto& region{ field.thresholds[i] };
			if (value <= region.minimum + 0.002f || value >= region.maximum - 0.002f) {
				continue;
			}
			PaintNoiseThreshold right{ region };
			right.enabled  = true;
			right.minimum  = value;
			region.maximum = value;
			field.thresholds.insert(
				field.thresholds.begin() + static_cast<std::ptrdiff_t>(i + 1), std::move(right)
			);
			return;
		}
	};
	auto remove_boundary = [&](int boundary) {
		if (boundary < 0 || boundary + 1 >= static_cast<int>(field.thresholds.size())) {
			return;
		}
		field.thresholds[static_cast<std::size_t>(boundary)].maximum =
			field.thresholds[static_cast<std::size_t>(boundary + 1)].maximum;
		field.thresholds.erase(field.thresholds.begin() + boundary + 1);
		normalize();
	};
	auto source_label = [&](const PaintNoiseThreshold& region) {
		if (!region.enabled) {
			return std::string{ "None" };
		}
		if (region.source_kind == PaintSourceKind::WeightedSet) {
			return layer.kind == SceneLayerKind::Tile
					 ? (region.weighted_tile_set_name.empty() ? std::string{ "<weighted set>" }
															  : region.weighted_tile_set_name)
					 : (region.weighted_prefab_set_name.empty() ? std::string{ "<weighted set>" }
																: region.weighted_prefab_set_name);
		}
		if (layer.kind == SceneLayerKind::Tile) {
			if (!region.tile.has_value()) {
				return std::string{ "None" };
			}
			return region.tile->texture.value + " [" + std::to_string(region.tile->slice.x) + "," +
				   std::to_string(region.tile->slice.y) + "]";
		}
		return region.prefab.has_value() ? PrefabDisplayName(*region.prefab)
										 : std::string{ "None" };
	};
	auto truncated = [](std::string label, float available) {
		if (available < 28.0f) {
			return std::string{};
		}
		const int maximum{ std::max(3, static_cast<int>(available / 7.0f)) };
		if (static_cast<int>(label.size()) <= maximum) {
			return label;
		}
		return label.substr(0, static_cast<std::size_t>(std::max(1, maximum - 3))) + "...";
	};

	normalize();
	const float width{ std::max(260.0f, ImGui::GetContentRegionAvail().x) };
	const float height{ 76.0f };
	const ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(
		"##recipe_noise_threshold_gradient", { width, height },
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
	);
	const ImVec2 mouse{ ImGui::GetIO().MousePos };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	const float bar_y0{ p0.y + 25.0f };
	const float bar_y1{ p0.y + 47.0f };

	for (int i{}; i < 96; ++i) {
		const float a{ static_cast<float>(i) / 96.0f };
		const float b{ static_cast<float>(i + 1) / 96.0f };
		const float value{ (a + b) * 0.5f };
		draw->AddRectFilled(
			{ p0.x + a * width, bar_y0 }, { p0.x + b * width + 1.0f, bar_y1 },
			ImGui::GetColorU32(ImVec4(value, value, value, 1.0f))
		);
	}
	draw->AddRect({ p0.x, bar_y0 }, { p0.x + width, bar_y1 }, ImGui::GetColorU32(ImGuiCol_Border));

	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		const auto& region{ field.thresholds[i] };
		const float x0{ p0.x + region.minimum * width };
		const float x1{ p0.x + region.maximum * width };
		if (region.enabled) {
			draw->AddRect(
				{ x0 + 1.0f, bar_y0 + 1.0f }, { x1 - 1.0f, bar_y1 - 1.0f },
				ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 0.95f)), 0.0f, 0, 2.0f
			);
		}
		const std::string label{ truncated(source_label(region), std::max(0.0f, x1 - x0 - 4.0f)) };
		if (!label.empty()) {
			const ImVec2 size{ ImGui::CalcTextSize(label.c_str()) };
			const float x{ std::clamp((x0 + x1 - size.x) * 0.5f, p0.x, p0.x + width - size.x) };
			const float y{ (i % 2 == 0) ? p0.y + 3.0f : bar_y1 + 6.0f };
			draw->AddText(
				{ x, y },
				ImGui::GetColorU32(region.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
				label.c_str()
			);
		}
	}

	for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
		const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
		draw->AddLine(
			{ x, bar_y0 - 5.0f }, { x, bar_y1 + 5.0f },
			ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f)), 2.0f
		);
		draw->AddTriangleFilled(
			{ x - 4.0f, bar_y0 - 6.0f }, { x + 4.0f, bar_y0 - 6.0f }, { x, bar_y0 - 1.0f },
			ImGui::GetColorU32(ImVec4(1.0f, 0.78f, 0.20f, 1.0f))
		);
	}

	static ImGuiID dragging_id{};
	static int dragging_boundary{ -1 };
	const ImGuiID widget_id{ ImGui::GetID("##recipe_noise_threshold_gradient") };
	auto nearest_boundary = [&] {
		int nearest{ -1 };
		float best{ 9.0f };
		for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
			const float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
			const float distance{ std::abs(mouse.x - x) };
			if (distance < best) {
				best	= distance;
				nearest = i;
			}
		}
		return nearest;
	};

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			dragging_id		  = widget_id;
			dragging_boundary = nearest;
		} else {
			split(std::clamp((mouse.x - p0.x) / width, 0.01f, 0.99f));
		}
	}
	if (dragging_id == widget_id && dragging_boundary >= 0 &&
		ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		const float value{ std::clamp((mouse.x - p0.x) / width, 0.0f, 1.0f) };
		const float low{ field.thresholds[static_cast<std::size_t>(dragging_boundary)].minimum +
						 0.005f };
		const float high{
			field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].maximum - 0.005f
		};
		const float boundary{ std::clamp(value, low, high) };
		field.thresholds[static_cast<std::size_t>(dragging_boundary)].maximum	  = boundary;
		field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].minimum = boundary;
	}
	if (dragging_id == widget_id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		dragging_id		  = 0;
		dragging_boundary = -1;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		const int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			remove_boundary(nearest);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Left-click empty gradient space: split a range.\n"
			"Left-drag a divider: move the threshold.\n"
			"Right-click a divider: remove it and merge its neighboring ranges."
		);
	}
}

void PaintEditor::DrawExtendedSourceRecipe(EditorContext& ctx, Scene&, SceneLayer& layer) {
	if (recipe_.source_kind == PaintSourceKind::WeightedSet) {
		if (layer.kind == SceneLayerKind::Tile) {
			DrawWeightedTileSetEditor(ctx);
		} else {
			DrawWeightedPrefabSetEditor(ctx);
		}
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Checkerboard) {
		ImGui::SeparatorText("Checkerboard Sources");

		if (layer.kind == SceneLayerKind::Tile) {
			std::optional<PaintTileSource> primary{
				tile_source_ ? std::optional<PaintTileSource>{ tile_source_ } : std::nullopt
			};

			BeginRecipeField("Primary", 260.0f);
			if (DrawTileSourceCombo(ctx, "##PrimaryCheckerTile", primary)) {
				tile_source_ = primary.value_or(PaintTileSource{});
			}

			BeginRecipeField("Secondary", 260.0f);
			DrawTileSourceCombo(ctx, "##SecondaryCheckerTile", recipe_.checker_tile);
		} else {
			std::optional<PrefabKey> primary{ prefab_source_
												  ? std::optional<PrefabKey>{ prefab_source_ }
												  : std::nullopt };

			BeginRecipeField("Primary", 260.0f);
			if (DrawPrefabSourceCombo(ctx, "##PrimaryCheckerPrefab", primary)) {
				prefab_source_ = primary.value_or(PrefabKey{});
			}

			BeginRecipeField("Secondary", 260.0f);
			DrawPrefabSourceCombo(ctx, "##SecondaryCheckerPrefab", recipe_.checker_prefab);
		}

		ImGui::TextDisabled(
			"Checkerboard alternates Primary and Secondary by raster-cell parity; "
			"Coverage is applied separately."
		);
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Autotile && layer.kind == SceneLayerKind::Tile) {
		DrawAutotileRuleSetEditor(ctx);
	}
}

void PaintEditor::DrawNoiseRecipe(EditorContext& ctx, Scene&, SceneLayer& layer) {
	ImGui::PushID("PaintNoiseSettings");
	ImGui::SeparatorText("Noise Source");

	int type_index{};
	if (recipe_.noise.type == NoiseType::Simplex) {
		type_index = 1;
	} else if (recipe_.noise.type == NoiseType::Value) {
		type_index = 2;
	}

	BeginRecipeField("Type");
	if (ImGui::Combo("##PaintNoiseType", &type_index, "Perlin\0Simplex\0Value\0")) {
		recipe_.noise.type = type_index == 0 ? NoiseType::Perlin
						   : type_index == 1 ? NoiseType::Simplex
											 : NoiseType::Value;
	}

	BeginRecipeField("Seed");
	ImGui::DragInt("##PaintNoiseSeed", &recipe_.noise.seed, 1.0f);

	BeginRecipeField("Frequency");
	ImGui::DragFloat(
		"##PaintNoiseFrequency", &recipe_.noise.frequency, 0.001f, 0.0001f, 1.0f, "%.4f",
		ImGuiSliderFlags_AlwaysClamp
	);

	BeginRecipeField("Octaves");
	ImGui::DragInt("##PaintNoiseOctaves", &recipe_.noise.octaves, 0.2f, 1, 12);

	BeginRecipeField("Lacunarity");
	ImGui::DragFloat(
		"##PaintNoiseLacunarity", &recipe_.noise.lacunarity, 0.02f, 1.0f, 8.0f, "%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);

	BeginRecipeField("Persistence");
	ImGui::SliderFloat("##PaintNoisePersistence", &recipe_.noise.persistence, 0.0f, 1.0f, "%.2f");

	float offset[2]{
		recipe_.noise.offset.x,
		recipe_.noise.offset.y,
	};
	BeginRecipeField("Noise Offset");
	if (ImGui::DragFloat2("##PaintNoiseOffset", offset, 0.25f)) {
		recipe_.noise.offset = { offset[0], offset[1] };
	}

	BeginRecipeField("Noise Preview");
	ImGui::Checkbox("##PaintNoisePreviewEnabled", &recipe_.show_noise_preview);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Overlay the raw grayscale noise only inside generator/paint geometry.");
	}

	if (recipe_.show_noise_preview) {
		BeginRecipeField("Preview Opacity");
		ImGui::SliderFloat(
			"##PaintNoisePreviewOpacity", &recipe_.noise_preview_alpha, 0.0f, 1.0f, "%.2f"
		);
	}

	BeginRecipeField(layer.kind == SceneLayerKind::Tile ? "Tile Preview" : "Entity Preview");
	ImGui::Checkbox("##PaintGeneratedPreviewEnabled", &recipe_.show_generated_preview);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Preview threshold-resolved output on top of the raw noise overlay.");
	}

	ImGui::SeparatorText("Noise Thresholds");
	DrawNoiseThresholdGradient(ctx, layer);

	int remove_region{ -1 };
	for (int i{}; i < static_cast<int>(recipe_.noise.thresholds.size()); ++i) {
		auto& region{ recipe_.noise.thresholds[static_cast<std::size_t>(i)] };

		ImGui::PushID(i);
		ImGui::Checkbox("##NoiseRangeEnabled", &region.enabled);
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Enable or disable output from this noise range.");
		}

		ImGui::SameLine();
		ImGui::TextDisabled("%.3f - %.3f", region.minimum, region.maximum);

		ImGui::SameLine();
		int kind{ !region.enabled									   ? 0
				  : region.source_kind == PaintSourceKind::WeightedSet ? 2
																	   : 1 };
		ImGui::SetNextItemWidth(112.0f);
		if (ImGui::Combo("##NoiseRangeKind", &kind, "None\0Single\0Weighted Set\0")) {
			region.enabled	   = kind != 0;
			region.source_kind = kind == 2 ? PaintSourceKind::WeightedSet : PaintSourceKind::Single;
		}

		if (region.enabled) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(220.0f);

			if (layer.kind == SceneLayerKind::Tile) {
				if (region.source_kind == PaintSourceKind::Single) {
					DrawTileSourceCombo(ctx, "##NoiseRangeTile", region.tile);
				} else {
					const auto* set{ FindWeightedTileSet(region.weighted_tile_set_name) };
					if (ImGui::BeginCombo(
							"##NoiseRangeTileSet", set ? set->name.c_str() : "<none>"
						)) {
						for (const auto& candidate : weighted_tile_sets_) {
							if (ImGui::Selectable(
									candidate.name.c_str(),
									candidate.name == region.weighted_tile_set_name
								)) {
								region.weighted_tile_set_name = candidate.name;
							}
						}
						ImGui::EndCombo();
					}
				}
			} else {
				if (region.source_kind == PaintSourceKind::Single) {
					DrawPrefabSourceCombo(ctx, "##NoiseRangePrefab", region.prefab);
				} else {
					const auto* set{ FindWeightedPrefabSet(region.weighted_prefab_set_name) };
					if (ImGui::BeginCombo(
							"##NoiseRangePrefabSet", set ? set->name.c_str() : "<none>"
						)) {
						for (const auto& candidate : weighted_prefab_sets_) {
							if (ImGui::Selectable(
									candidate.name.c_str(),
									candidate.name == region.weighted_prefab_set_name
								)) {
								region.weighted_prefab_set_name = candidate.name;
							}
						}
						ImGui::EndCombo();
					}
				}
			}

			ImGui::SameLine();
			ImGui::TextDisabled("Origin");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(100.0f);
			DrawOriginCombo("##NoiseRangeOrigin", region.origin);
		}

		if (recipe_.noise.thresholds.size() > 1) {
			ImGui::SameLine();
			if (ImGui::SmallButton("x")) {
				remove_region = i;
			}
		}
		ImGui::PopID();
	}

	if (remove_region >= 0 && recipe_.noise.thresholds.size() > 1) {
		const int boundary{
			std::clamp(remove_region, 0, static_cast<int>(recipe_.noise.thresholds.size()) - 2)
		};

		recipe_.noise.thresholds[static_cast<std::size_t>(boundary)].maximum =
			recipe_.noise.thresholds[static_cast<std::size_t>(boundary + 1)].maximum;

		recipe_.noise.thresholds.erase(recipe_.noise.thresholds.begin() + boundary + 1);
	}

	ImGui::TextDisabled("Gradient ranges output None, Single, or a reusable Weighted Set.");
	ImGui::PopID();
}

PaintEditor::TileSliceSettings& PaintEditor::GetSliceSettings(
	EditorContext& ctx, const TextureKey& texture
) {
	auto [it, inserted]{ tile_slice_settings_.try_emplace(texture.value) };
	if (inserted) {
		for (const auto& record :
			 ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets()) {
			if (record.kind == AssetKind::Texture &&
				record.key == static_cast<const AssetKey&>(texture)) {
				if (record.metadata.dimensions.has_value() &&
					record.metadata.dimensions->IsPositive()) {
					it->second.tile_size = *record.metadata.dimensions;
				}
				break;
			}
		}
	}
	return it->second;
}

const PaintEditor::TileSliceSettings* PaintEditor::FindSliceSettings(
	const TextureKey& texture
) const {
	const auto it{ tile_slice_settings_.find(texture.value) };
	return it == tile_slice_settings_.end() ? nullptr : &it->second;
}

PaintTileSource PaintEditor::MakeTileSource(
	EditorContext& ctx, const TextureKey& texture, V2_int slice
) const {
	const auto* settings{ FindSliceSettings(texture) };
	if (!settings) {
		return {};
	}
	V2_int texture_size{};
	for (const auto& record :
		 ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets()) {
		if (record.kind == AssetKind::Texture &&
			record.key == static_cast<const AssetKey&>(texture)) {
			if (record.metadata.dimensions.has_value()) {
				texture_size = *record.metadata.dimensions;
			}
			break;
		}
	}
	if (!texture_size.IsPositive() && ctx.editor.GetAssetManager().Has(texture)) {
		texture_size = ctx.editor.GetAssetManager().GetTextureSize(texture);
	}
	if (!texture_size.IsPositive() || !settings->tile_size.IsPositive()) {
		return {};
	}

	const V2_int pixel_min{
		settings->margin.x + slice.x * (settings->tile_size.x + settings->spacing.x),
		settings->margin.y + slice.y * (settings->tile_size.y + settings->spacing.y),
	};
	const V2_int pixel_max{ pixel_min + settings->tile_size };
	if (pixel_min.x < 0 || pixel_min.y < 0 || pixel_max.x > texture_size.x ||
		pixel_max.y > texture_size.y) {
		return {};
	}
	const V2_float uv0{ static_cast<float>(pixel_min.x) / static_cast<float>(texture_size.x),
						static_cast<float>(pixel_min.y) / static_cast<float>(texture_size.y) };
	const V2_float uv1{ static_cast<float>(pixel_max.x) / static_cast<float>(texture_size.x),
						static_cast<float>(pixel_max.y) / static_cast<float>(texture_size.y) };
	return PaintTileSource{
		.texture			 = texture,
		.texture_coordinates = { V2_float{ uv0.x, uv0.y }, V2_float{ uv1.x, uv0.y },
								 V2_float{ uv1.x, uv1.y }, V2_float{ uv0.x, uv1.y } },
		.pixel_size			 = settings->tile_size,
		.slice				 = slice,
	};
}

std::string PaintEditor::TileEntryId(const TextureKey& texture, V2_int slice) const {
	return texture.value + "#" + std::to_string(slice.x) + "," + std::to_string(slice.y);
}

PaintEditor::TileLibraryEntry* PaintEditor::FindTileEntry(std::string_view id) {
	const auto it{ std::ranges::find(tile_library_, id, &TileLibraryEntry::id) };
	return it == tile_library_.end() ? nullptr : &*it;
}

const PaintEditor::TileLibraryEntry* PaintEditor::FindTileEntry(std::string_view id) const {
	const auto it{ std::ranges::find(tile_library_, id, &TileLibraryEntry::id) };
	return it == tile_library_.end() ? nullptr : &*it;
}

PaintWeightedTileSet* PaintEditor::FindWeightedTileSet(std::string_view name) {
	const auto it{ std::ranges::find(weighted_tile_sets_, name, &PaintWeightedTileSet::name) };
	return it == weighted_tile_sets_.end() ? nullptr : &*it;
}

const PaintWeightedTileSet* PaintEditor::FindWeightedTileSet(std::string_view name) const {
	const auto it{ std::ranges::find(weighted_tile_sets_, name, &PaintWeightedTileSet::name) };
	return it == weighted_tile_sets_.end() ? nullptr : &*it;
}

PaintWeightedPrefabSet* PaintEditor::FindWeightedPrefabSet(std::string_view name) {
	const auto it{ std::ranges::find(weighted_prefab_sets_, name, &PaintWeightedPrefabSet::name) };
	return it == weighted_prefab_sets_.end() ? nullptr : &*it;
}

const PaintWeightedPrefabSet* PaintEditor::FindWeightedPrefabSet(std::string_view name) const {
	const auto it{ std::ranges::find(weighted_prefab_sets_, name, &PaintWeightedPrefabSet::name) };
	return it == weighted_prefab_sets_.end() ? nullptr : &*it;
}

PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::string_view name) {
	const auto it{ std::ranges::find(autotile_rule_sets_, name, &PaintAutotileRuleSet::name) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

const PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::string_view name) const {
	const auto it{ std::ranges::find(autotile_rule_sets_, name, &PaintAutotileRuleSet::name) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::uint64_t id) {
	const auto it{ std::ranges::find(autotile_rule_sets_, id, &PaintAutotileRuleSet::id) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

const PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::uint64_t id) const {
	const auto it{ std::ranges::find(autotile_rule_sets_, id, &PaintAutotileRuleSet::id) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

std::string PaintEditor::UniqueWeightedTileSetName() const {
	for (int number{ 1 };; ++number) {
		std::string candidate{ "Weighted Set " + std::to_string(number) };
		if (!FindWeightedTileSet(candidate)) {
			return candidate;
		}
	}
}

std::string PaintEditor::UniqueWeightedPrefabSetName() const {
	for (int number{ 1 };; ++number) {
		std::string candidate{ "Weighted Set " + std::to_string(number) };
		if (!FindWeightedPrefabSet(candidate)) {
			return candidate;
		}
	}
}

std::string PaintEditor::UniqueAutotileRuleSetName() const {
	for (int number{ 1 };; ++number) {
		std::string candidate{ "Terrain " + std::to_string(number) };
		if (!FindAutotileRuleSet(candidate)) {
			return candidate;
		}
	}
}

std::uint64_t PaintEditor::NextAutotileRuleSetId() const {
	std::uint64_t next{ 1 };
	for (const auto& rules : autotile_rule_sets_) {
		next = std::max(next, rules.id + 1);
	}
	return next;
}

void PaintEditor::RecomputeAutotileAround(
	Tilemap tilemap, V2_int cell, const PaintAutotileRuleSet& rules
) {
	if (!tilemap || rules.id == 0) {
		return;
	}

	auto set_display = [&](V2_int display_cell, const PaintAutotileRuleSet& display_rules,
						   int index, V2_float offset) {
		if (index < 0 || index >= static_cast<int>(display_rules.tiles.size()) ||
			!display_rules.tiles[static_cast<std::size_t>(index)].has_value()) {
			if (const TilemapTile* current{ tilemap.FindTile(display_cell) };
				current && current->terrain_ruleset_id.has_value()) {
				tilemap.EraseTile(display_cell);
			}
			return;
		}
		const PaintTileSource& source{ *display_rules.tiles[static_cast<std::size_t>(index)] };
		tilemap.SetTile(
			TilemapTile{
				.coordinate			 = display_cell,
				.texture			 = source.texture,
				.texture_coordinates = source.texture_coordinates,
				.pixel_size			 = source.pixel_size,
				.origin				 = Origin::TopLeft,
				.offset				 = offset,
				.tint				 = color::White,
				.terrain_ruleset_id	 = display_rules.id,
			}
		);
	};

	if (rules.format == PaintAutotileFormat::DualGrid16) {
		static constexpr std::array<V2_int, 4> affected{ V2_int{ 0, 0 }, V2_int{ -1, 0 },
														 V2_int{ 0, -1 }, V2_int{ -1, -1 } };
		const V2_float half_cell{ tilemap.GetData().cell_size * 0.5f };
		for (const V2_int delta : affected) {
			const V2_int display_cell{ cell + delta };
			const std::array<V2_int, 4> logical{
				display_cell,
				display_cell + V2_int{ 1, 0 },
				display_cell + V2_int{ 0, 1 },
				display_cell + V2_int{ 1, 1 },
			};
			std::optional<std::uint64_t> display_ruleset;
			for (const V2_int logical_cell : logical) {
				if (const auto id{ tilemap.GetTerrainRuleset(logical_cell) }) {
					display_ruleset = *id;
					break;
				}
			}
			if (!display_ruleset.has_value()) {
				if (const TilemapTile* current{ tilemap.FindTile(display_cell) };
					current && current->terrain_ruleset_id.has_value()) {
					tilemap.EraseTile(display_cell);
				}
				continue;
			}
			const PaintAutotileRuleSet* display_rules{ FindAutotileRuleSet(*display_ruleset) };
			if (!display_rules || display_rules->format != PaintAutotileFormat::DualGrid16) {
				continue;
			}
			int mask{};
			for (int i{}; i < 4; ++i) {
				if (tilemap.GetTerrainRuleset(logical[static_cast<std::size_t>(i)]) ==
					display_rules->id) {
					mask |= 1 << i;
				}
			}
			set_display(display_cell, *display_rules, mask, half_cell);
		}
		return;
	}

	static constexpr std::array<V2_int, 9> offsets{
		V2_int{ 0, 0 },	 V2_int{ 0, -1 }, V2_int{ 1, 0 },  V2_int{ 0, 1 },	 V2_int{ -1, 0 },
		V2_int{ 1, -1 }, V2_int{ 1, 1 },  V2_int{ -1, 1 }, V2_int{ -1, -1 },
	};
	auto occupied = [&](V2_int candidate) {
		return tilemap.GetTerrainRuleset(candidate) == rules.id;
	};
	for (const V2_int delta : offsets) {
		const V2_int candidate{ cell + delta };
		if (!occupied(candidate)) {
			if (const TilemapTile* current{ tilemap.FindTile(candidate) };
				current && current->terrain_ruleset_id == rules.id) {
				tilemap.EraseTile(candidate);
			}
			continue;
		}
		const int index{ AutotileIndex(rules.format, occupied, candidate) };
		set_display(candidate, rules, index, {});
	}
}

void PaintEditor::DrawTileSourceBrowser(EditorContext& ctx, PaintTileSource& source) {
	EnsureProjectLibrary(ctx);

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##PaintTileSourceSearch", "Search tile name or group...", &tile_source_search_
	);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Filter paint sources by tile name or tile group. "
			"This does not change the Tiles asset selection."
		);
	}

	const TileLibraryEntry* selected_entry{};
	for (const auto& entry : tile_library_) {
		const PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
		if (candidate && source && candidate == source) {
			selected_entry = &entry;
			break;
		}
	}

	ImGui::TextDisabled(
		"Selected: %s", selected_entry ? selected_entry->name.c_str()
						: source	   ? source.texture.value.c_str()
									   : "<none>"
	);

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	auto record_for = [&](const TextureKey& texture) {
		return std::ranges::find_if(records, [&](const auto& record) {
			return record.kind == AssetKind::Texture &&
				   record.key == static_cast<const AssetKey&>(texture);
		});
	};

	std::vector<std::string> groups{ TileGroupNames() };
	SortGroupsUngroupedFirst(groups);

	for (const std::string& group : groups) {
		const bool group_match{ ContainsInsensitive(group, tile_source_search_) };
		const bool any_match{ std::ranges::any_of(
			tile_library_, [&](const TileLibraryEntry& entry) {
				return entry.group == group &&
					   (group_match || ContainsInsensitive(entry.name, tile_source_search_) ||
						ContainsInsensitive(entry.texture.value, tile_source_search_));
			}
		) };
		if (!any_match) {
			continue;
		}

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_OpenOnArrow };
		if (!tile_source_search_.empty()) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}

		ImGui::PushID(group.c_str());
		const bool open{ ImGui::TreeNodeEx("##PaintTileSourceGroup", flags, "%s", group.c_str()) };

		if (open) {
			constexpr float image_side{ 40.0f };
			const float row_right{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
			bool first_on_row{ true };

			for (const auto& entry : tile_library_) {
				if (entry.group != group) {
					continue;
				}
				if (!group_match && !ContainsInsensitive(entry.name, tile_source_search_) &&
					!ContainsInsensitive(entry.texture.value, tile_source_search_)) {
					continue;
				}

				const PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
				if (!candidate) {
					continue;
				}

				if (!first_on_row) {
					const float next_right{ ImGui::GetItemRectMax().x +
											ImGui::GetStyle().ItemSpacing.x + image_side };
					if (next_right <= row_right) {
						ImGui::SameLine();
					}
				}

				ImGui::PushID(entry.id.c_str());

				const ImVec2 p0{ ImGui::GetCursorScreenPos() };
				ImGui::InvisibleButton("##PaintTileSource", { image_side, image_side });

				const bool selected{ source && source == candidate };
				const bool hovered{ ImGui::IsItemHovered() };
				auto* draw{ ImGui::GetWindowDrawList() };
				const auto record{ record_for(entry.texture) };

				if (record != records.end() && record->preview.has_value()) {
					const auto& uv{ candidate.texture_coordinates };
					const ImVec2 p1{ p0.x + image_side, p0.y + image_side };

					draw->AddImageQuad(
						static_cast<ImTextureID>(record->preview->texture), p0, { p1.x, p0.y }, p1,
						{ p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]),
						ToImGui(uv[3])
					);
				} else {
					draw->AddRectFilled(
						p0, { p0.x + image_side, p0.y + image_side },
						ImGui::GetColorU32(ImGuiCol_FrameBg)
					);
				}

				draw->AddRect(
					p0, { p0.x + image_side, p0.y + image_side },
					selected
						? kPreview
						: ImGui::GetColorU32(hovered ? ImGuiCol_BorderShadow : ImGuiCol_Border),
					2.0f, 0, selected ? 2.0f : 1.0f
				);

				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
					source = candidate;
				}
				if (hovered) {
					ImGui::SetTooltip("%s", entry.name.c_str());
				}

				ImGui::PopID();
				first_on_row = false;
			}

			ImGui::TreePop();
		}

		ImGui::PopID();
	}

	if (tile_library_.empty()) {
		ImGui::TextDisabled("No tiles are available. Import tiles from the Tiles tab.");
	}
}

bool PaintEditor::DrawTileSourceCombo(
	EditorContext& ctx, const char* id, std::optional<PaintTileSource>& source
) {
	EnsureProjectLibrary(ctx);

	std::string preview{ "<none>" };
	if (source.has_value()) {
		if (const auto it{ std::ranges::find_if(
				tile_library_,
				[&](const TileLibraryEntry& entry) {
					const PaintTileSource candidate{
						MakeTileSource(ctx, entry.texture, entry.slice)
					};
					return candidate && candidate == *source;
				}
			) };
			it != tile_library_.end()) {
			preview = it->name;
		} else {
			preview = source->texture.value;
		}
	}

	const float requested_width{ ImGui::CalcItemWidth() };
	const float thumbnail_side{ ImGui::GetFrameHeight() };
	const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };

	if (source.has_value() && *source) {
		DrawTileThumbnail(ctx, *source, thumbnail_side);
	} else {
		const ImVec2 p0{ ImGui::GetCursorScreenPos() };
		ImGui::Dummy({ thumbnail_side, thumbnail_side });
		ImGui::GetWindowDrawList()->AddRectFilled(
			p0,
			{
				p0.x + thumbnail_side,
				p0.y + thumbnail_side,
			},
			ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding
		);
		ImGui::GetWindowDrawList()->AddRect(
			p0,
			{
				p0.x + thumbnail_side,
				p0.y + thumbnail_side,
			},
			ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding
		);
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::SetNextItemWidth(std::max(1.0f, requested_width - thumbnail_side - spacing));

	bool changed{};
	if (!ImGui::BeginCombo(id, preview.c_str())) {
		return false;
	}

	ImGui::SetNextItemWidth(320.0f);
	ImGui::InputTextWithHint(
		"##TileSourceComboSearch", "Search tile name or group...", &tile_source_search_
	);

	if (ImGui::Selectable("<none>", !source.has_value())) {
		source.reset();
		changed = true;
	}

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	std::vector<std::string> groups{ TileGroupNames() };
	SortGroupsUngroupedFirst(groups);

	for (const std::string& group : groups) {
		const bool group_match{ ContainsInsensitive(group, tile_source_search_) };
		bool any{};

		for (const auto& entry : tile_library_) {
			if (entry.group == group &&
				(group_match || ContainsInsensitive(entry.name, tile_source_search_))) {
				any = true;
				break;
			}
		}
		if (!any) {
			continue;
		}

		ImGui::SeparatorText(group.c_str());

		constexpr float image_side{ 30.0f };
		const float row_right{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
		bool first_on_row{ true };

		for (const auto& entry : tile_library_) {
			if (entry.group != group ||
				(!group_match && !ContainsInsensitive(entry.name, tile_source_search_))) {
				continue;
			}

			PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
			if (!candidate) {
				continue;
			}

			if (!first_on_row) {
				const float next_right{ ImGui::GetItemRectMax().x +
										ImGui::GetStyle().ItemSpacing.x + image_side };
				if (next_right <= row_right) {
					ImGui::SameLine();
				}
			}

			ImGui::PushID(entry.id.c_str());
			const ImVec2 p0{ ImGui::GetCursorScreenPos() };
			ImGui::InvisibleButton("##TileSourceThumb", { image_side, image_side });

			const bool selected{ source.has_value() && *source == candidate };
			const bool hovered{ ImGui::IsItemHovered() };
			auto* draw{ ImGui::GetWindowDrawList() };

			const auto record{ std::ranges::find_if(records, [&](const auto& asset) {
				return asset.kind == AssetKind::Texture &&
					   asset.key == static_cast<const AssetKey&>(entry.texture);
			}) };

			if (record != records.end() && record->preview.has_value()) {
				const auto& uv{ candidate.texture_coordinates };
				const ImVec2 p1{ p0.x + image_side, p0.y + image_side };

				draw->AddImageQuad(
					static_cast<ImTextureID>(record->preview->texture), p0, { p1.x, p0.y }, p1,
					{ p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]), ToImGui(uv[3])
				);
			} else {
				draw->AddRectFilled(
					p0,
					{
						p0.x + image_side,
						p0.y + image_side,
					},
					ImGui::GetColorU32(ImGuiCol_FrameBg)
				);
			}

			draw->AddRect(
				p0,
				{
					p0.x + image_side,
					p0.y + image_side,
				},
				selected ? kPreview
						 : ImGui::GetColorU32(hovered ? ImGuiCol_HeaderHovered : ImGuiCol_Border),
				1.0f, 0, selected ? 2.0f : 1.0f
			);

			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				source	= candidate;
				changed = true;
				ImGui::CloseCurrentPopup();
			}
			if (hovered) {
				ImGui::SetTooltip("%s", entry.name.c_str());
			}

			ImGui::PopID();
			first_on_row = false;
		}
	}

	ImGui::EndCombo();
	return changed;
}

void PaintEditor::DrawPrefabSourceBrowser(EditorContext& ctx, PrefabKey& source) {
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint(
		"##PaintPrefabSourceSearch", "Search prefab name or group...", &prefab_source_search_
	);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Filter paint sources by prefab name or prefab group. This does not change Prefab "
			"Inspector selection."
		);
	}
	ImGui::TextDisabled("Selected: %s", source ? PrefabDisplayName(source).c_str() : "<none>");

	auto keys{ ctx.editor.GetAssetManager().GetPrefabKeys() };
	std::ranges::sort(keys, {}, [](const PrefabKey& key) { return key.value; });
	std::vector<std::string> groups;
	for (const PrefabKey& key : keys) {
		const std::string group{ GetPrefabGroup(ctx, key) };
		if (ContainsInsensitive(PrefabDisplayName(key), prefab_source_search_) ||
			ContainsInsensitive(group, prefab_source_search_)) {
			groups.push_back(group);
		}
	}
	SortGroupsUngroupedFirst(groups);
	for (const std::string& group : groups) {
		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_OpenOnArrow };
		if (!prefab_source_search_.empty()) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		ImGui::PushID(group.c_str());
		if (ImGui::TreeNodeEx("##PaintPrefabSourceGroup", flags, "%s", group.c_str())) {
			for (const PrefabKey& key : keys) {
				if (GetPrefabGroup(ctx, key) != group) {
					continue;
				}
				const std::string name{ PrefabDisplayName(key) };
				if (!ContainsInsensitive(name, prefab_source_search_) &&
					!ContainsInsensitive(group, prefab_source_search_)) {
					continue;
				}
				if (ImGui::Selectable(name.c_str(), key == source)) {
					source = key;
				}
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (keys.empty()) {
		ImGui::TextDisabled("No prefab assets are loaded.");
	}
}

bool PaintEditor::DrawPrefabSourceCombo(
	EditorContext& ctx, const char* id, std::optional<PrefabKey>& source
) {
	const std::string preview{ source.has_value() ? PrefabDisplayName(*source)
												  : std::string{ "<none>" } };
	bool changed{};
	if (!ImGui::BeginCombo(id, preview.c_str())) {
		return false;
	}
	ImGui::SetNextItemWidth(300.0f);
	ImGui::InputTextWithHint(
		"##PrefabSourceComboSearch", "Search prefab name or group...", &prefab_source_search_
	);
	if (ImGui::Selectable("<none>", !source.has_value())) {
		source.reset();
		changed = true;
	}
	auto keys{ ctx.editor.GetAssetManager().GetPrefabKeys() };
	std::ranges::sort(keys, {}, [](const PrefabKey& key) { return key.value; });
	std::vector<std::string> groups;
	for (const PrefabKey& key : keys) {
		const std::string group{ GetPrefabGroup(ctx, key) };
		if (ContainsInsensitive(PrefabDisplayName(key), prefab_source_search_) ||
			ContainsInsensitive(group, prefab_source_search_)) {
			groups.push_back(group);
		}
	}
	SortGroupsUngroupedFirst(groups);
	for (const std::string& group : groups) {
		ImGui::SeparatorText(group.c_str());
		for (const PrefabKey& key : keys) {
			if (GetPrefabGroup(ctx, key) != group) {
				continue;
			}
			const std::string name{ PrefabDisplayName(key) };
			if (!ContainsInsensitive(name, prefab_source_search_) &&
				!ContainsInsensitive(group, prefab_source_search_)) {
				continue;
			}
			const bool selected{ source.has_value() && *source == key };
			if (ImGui::Selectable(name.c_str(), selected)) {
				source	= key;
				changed = true;
			}
		}
	}
	ImGui::EndCombo();
	return changed;
}

void PaintEditor::DrawWeightedTileSetEditor(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);

	PaintWeightedTileSet* active{ FindWeightedTileSet(recipe_.weighted_tile_set_name) };

	BeginRecipeField("Weighted Set");
	if (ImGui::BeginCombo(
			"##PaintRecipeWeightedTileSet", active ? active->name.c_str() : "<none>"
		)) {
		for (const auto& set : weighted_tile_sets_) {
			if (ImGui::Selectable(set.name.c_str(), active && set.name == active->name)) {
				recipe_.weighted_tile_set_name = set.name;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Add Weighted Set##Tile")) {
		PaintWeightedTileSet set{ .name = UniqueWeightedTileSetName() };
		recipe_.weighted_tile_set_name = set.name;
		weighted_tile_sets_.push_back(std::move(set));
		SaveProjectLibrary(ctx);
		active = FindWeightedTileSet(recipe_.weighted_tile_set_name);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!active);
	if (ImGui::Button("Delete Set##Tile") && active) {
		const std::string name{ active->name };
		std::erase_if(weighted_tile_sets_, [&](const PaintWeightedTileSet& set) {
			return set.name == name;
		});
		recipe_.weighted_tile_set_name =
			weighted_tile_sets_.empty() ? std::string{} : weighted_tile_sets_.front().name;
		SaveProjectLibrary(ctx);
		active = FindWeightedTileSet(recipe_.weighted_tile_set_name);
	}
	ImGui::EndDisabled();

	if (!active) {
		ImGui::TextDisabled("No weighted tile set selected.");
		return;
	}

	std::string name{ active->name };
	BeginRecipeField("Set Name", 240.0f);
	if (ImGui::InputText("##WeightedTileSetName", &name) && !name.empty() && name != active->name &&
		!FindWeightedTileSet(name)) {
		const std::string old{ active->name };
		active->name				   = name;
		recipe_.weighted_tile_set_name = name;

		for (auto& threshold : recipe_.noise.thresholds) {
			if (threshold.weighted_tile_set_name == old) {
				threshold.weighted_tile_set_name = name;
			}
		}
		SaveProjectLibrary(ctx);
	}

	if (ImGui::BeginTable(
			"##WeightedTileMembers", 3,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Tile", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 28.0f);
		ImGui::TableHeadersRow();

		int remove{ -1 };
		for (int i{}; i < static_cast<int>(active->entries.size()); ++i) {
			auto& entry{ active->entries[static_cast<std::size_t>(i)] };

			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			{
				std::string display{ entry.source.texture.value };
				if (const auto it{ std::ranges::find_if(
						tile_library_,
						[&](const TileLibraryEntry& candidate) {
							const PaintTileSource resolved{
								MakeTileSource(ctx, candidate.texture, candidate.slice)
							};
							return resolved && resolved == entry.source;
						}
					) };
					it != tile_library_.end()) {
					display = it->name;
				}

				DrawTileThumbnail(ctx, entry.source, ImGui::GetFrameHeight());
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(display.c_str());
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Weight", &entry.weight, 0.05f, 0.0f, 100.0f, "%.2f")) {
				entry.weight = std::max(0.0f, entry.weight);
				SaveProjectLibrary(ctx);
			}

			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("x")) {
				remove = i;
			}
			ImGui::PopID();
		}

		if (remove >= 0) {
			active->entries.erase(active->entries.begin() + remove);
			SaveProjectLibrary(ctx);
		}
		ImGui::EndTable();
	}

	if (ImGui::Button("Add Tile Sources...")) {
		ImGui::OpenPopup("AddWeightedTileSources");
	}
	if (ImGui::BeginPopup("AddWeightedTileSources")) {
		ImGui::SetNextItemWidth(340.0f);
		ImGui::InputTextWithHint(
			"##AddWeightedTileSearch", "Search tile name or group...", &tile_source_search_
		);

		for (const std::string& group : TileGroupNames()) {
			const bool group_match{ ContainsInsensitive(group, tile_source_search_) };
			if (!ImGui::TreeNode(group.c_str())) {
				continue;
			}

			for (const auto& tile : tile_library_) {
				if (tile.group != group ||
					(!group_match && !ContainsInsensitive(tile.name, tile_source_search_))) {
					continue;
				}

				PaintTileSource candidate{ MakeTileSource(ctx, tile.texture, tile.slice) };
				if (!candidate) {
					continue;
				}

				const bool exists{ std::ranges::any_of(
					active->entries,
					[&](const PaintWeightedTileEntry& entry) { return entry.source == candidate; }
				) };

				ImGui::BeginDisabled(exists);
				ImGui::PushID(tile.id.c_str());
				DrawTileThumbnail(ctx, candidate, ImGui::GetFrameHeight());
				ImGui::SameLine();
				if (ImGui::Button(
						"+", {
								 ImGui::GetFrameHeight(),
								 ImGui::GetFrameHeight(),
							 }
					)) {
					active->entries.push_back({ candidate, 1.0f });
					SaveProjectLibrary(ctx);
				}
				ImGui::SameLine();
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(tile.name.c_str());
				ImGui::PopID();
				ImGui::EndDisabled();
			}

			ImGui::TreePop();
		}

		ImGui::EndPopup();
	}
}

void PaintEditor::DrawWeightedPrefabSetEditor(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);

	PaintWeightedPrefabSet* active{ FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) };

	BeginRecipeField("Weighted Set");
	if (ImGui::BeginCombo(
			"##PaintRecipeWeightedPrefabSet", active ? active->name.c_str() : "<none>"
		)) {
		for (const auto& set : weighted_prefab_sets_) {
			if (ImGui::Selectable(set.name.c_str(), active && set.name == active->name)) {
				recipe_.weighted_prefab_set_name = set.name;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("Add Weighted Set##Prefab")) {
		PaintWeightedPrefabSet set{ .name = UniqueWeightedPrefabSetName() };
		recipe_.weighted_prefab_set_name = set.name;
		weighted_prefab_sets_.push_back(std::move(set));
		SaveProjectLibrary(ctx);
		active = FindWeightedPrefabSet(recipe_.weighted_prefab_set_name);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!active);
	if (ImGui::Button("Delete Set##Prefab") && active) {
		const std::string name{ active->name };
		std::erase_if(weighted_prefab_sets_, [&](const auto& set) { return set.name == name; });
		recipe_.weighted_prefab_set_name =
			weighted_prefab_sets_.empty() ? std::string{} : weighted_prefab_sets_.front().name;
		SaveProjectLibrary(ctx);
		active = FindWeightedPrefabSet(recipe_.weighted_prefab_set_name);
	}
	ImGui::EndDisabled();

	if (!active) {
		ImGui::TextDisabled("No weighted prefab set selected.");
		return;
	}

	std::string name{ active->name };
	BeginRecipeField("Set Name", 240.0f);
	if (ImGui::InputText("##WeightedPrefabSetName", &name) && !name.empty() &&
		name != active->name && !FindWeightedPrefabSet(name)) {
		const std::string old{ active->name };
		active->name					 = name;
		recipe_.weighted_prefab_set_name = name;

		for (auto& threshold : recipe_.noise.thresholds) {
			if (threshold.weighted_prefab_set_name == old) {
				threshold.weighted_prefab_set_name = name;
			}
		}
		SaveProjectLibrary(ctx);
	}

	if (ImGui::BeginTable(
			"##WeightedPrefabMembers", 3,
			ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV |
				ImGuiTableFlags_SizingStretchProp
		)) {
		ImGui::TableSetupColumn("Prefab", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Weight", ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 28.0f);
		ImGui::TableHeadersRow();

		int remove{ -1 };
		for (int i{}; i < static_cast<int>(active->entries.size()); ++i) {
			auto& entry{ active->entries[static_cast<std::size_t>(i)] };

			ImGui::PushID(i);
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(PrefabDisplayName(entry.prefab).c_str());

			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (ImGui::DragFloat("##Weight", &entry.weight, 0.05f, 0.0f, 100.0f, "%.2f")) {
				entry.weight = std::max(0.0f, entry.weight);
				SaveProjectLibrary(ctx);
			}

			ImGui::TableSetColumnIndex(2);
			if (ImGui::SmallButton("x")) {
				remove = i;
			}
			ImGui::PopID();
		}

		if (remove >= 0) {
			active->entries.erase(active->entries.begin() + remove);
			SaveProjectLibrary(ctx);
		}
		ImGui::EndTable();
	}

	if (ImGui::Button("Add Prefab Sources...")) {
		ImGui::OpenPopup("AddWeightedPrefabSources");
	}

	if (ImGui::BeginPopup("AddWeightedPrefabSources")) {
		ImGui::SetNextItemWidth(340.0f);
		ImGui::InputTextWithHint(
			"##AddWeightedPrefabSearch", "Search prefab name or group...", &prefab_source_search_
		);

		auto keys{ ctx.editor.GetAssetManager().GetPrefabKeys() };
		std::vector<std::string> groups;
		for (const auto& key : keys) {
			const std::string group{ GetPrefabGroup(ctx, key) };
			if (ContainsInsensitive(PrefabDisplayName(key), prefab_source_search_) ||
				ContainsInsensitive(group, prefab_source_search_)) {
				groups.push_back(group);
			}
		}
		SortGroupsUngroupedFirst(groups);

		for (const auto& group : groups) {
			if (!ImGui::TreeNode(group.c_str())) {
				continue;
			}

			for (const auto& key : keys) {
				if (GetPrefabGroup(ctx, key) != group) {
					continue;
				}

				const std::string display{ PrefabDisplayName(key) };
				if (!ContainsInsensitive(display, prefab_source_search_) &&
					!ContainsInsensitive(group, prefab_source_search_)) {
					continue;
				}

				const bool exists{ std::ranges::any_of(active->entries, [&](const auto& entry) {
					return entry.prefab == key;
				}) };

				ImGui::BeginDisabled(exists);
				ImGui::PushID(key.value.c_str());
				if (ImGui::SmallButton("+")) {
					active->entries.push_back({ key, 1.0f });
					SaveProjectLibrary(ctx);
				}
				ImGui::SameLine();
				ImGui::TextUnformatted(display.c_str());
				ImGui::PopID();
				ImGui::EndDisabled();
			}

			ImGui::TreePop();
		}

		ImGui::EndPopup();
	}
}

void PaintEditor::DrawAutotileRuleSetEditor(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);

	PaintAutotileRuleSet* active{ FindAutotileRuleSet(recipe_.autotile_ruleset_name) };

	BeginRecipeField("Ruleset");
	if (ImGui::BeginCombo("##PaintAutotileRuleset", active ? active->name.c_str() : "<none>")) {
		for (const auto& set : autotile_rule_sets_) {
			if (ImGui::Selectable(set.name.c_str(), active && set.name == active->name)) {
				recipe_.autotile_ruleset_name = set.name;
			}
		}
		ImGui::EndCombo();
	}

	ImGui::SameLine();
	if (ImGui::Button("+ Ruleset")) {
		PaintAutotileRuleSet set;
		set.id	 = NextAutotileRuleSetId();
		set.name = UniqueAutotileRuleSetName();
		set.tiles.resize(16);

		recipe_.autotile_ruleset_name = set.name;
		autotile_rule_sets_.push_back(std::move(set));
		SaveProjectLibrary(ctx);
		active = FindAutotileRuleSet(recipe_.autotile_ruleset_name);
	}

	ImGui::SameLine();
	ImGui::BeginDisabled(!active);
	if (ImGui::Button("Delete Ruleset") && active) {
		const std::string name{ active->name };
		std::erase_if(autotile_rule_sets_, [&](const auto& set) { return set.name == name; });
		recipe_.autotile_ruleset_name =
			autotile_rule_sets_.empty() ? std::string{} : autotile_rule_sets_.front().name;
		SaveProjectLibrary(ctx);
		active = FindAutotileRuleSet(recipe_.autotile_ruleset_name);
	}
	ImGui::EndDisabled();

	if (!active) {
		ImGui::TextDisabled("No autotile ruleset exists. Create one with + Ruleset.");
		return;
	}

	std::string name{ active->name };
	BeginRecipeField("Name", 220.0f);
	if (ImGui::InputText("##AutotileName", &name) && !name.empty() && name != active->name &&
		!FindAutotileRuleSet(name)) {
		active->name				  = name;
		recipe_.autotile_ruleset_name = name;
		SaveProjectLibrary(ctx);
	}

	int format{ static_cast<int>(active->format) };
	BeginRecipeField("Format", 220.0f);
	if (ImGui::Combo(
			"##AutotileFormat", &format,
			"Classic 15\0Blob 47 (8-neighbor)\0"
			"4-neighbor / Subset 16\0Dual Grid 16\0Wang 16\0"
		)) {
		active->format = static_cast<PaintAutotileFormat>(format);
		active->tiles.resize(static_cast<std::size_t>(RequiredAutotileTileCount(active->format)));
		SaveProjectLibrary(ctx);
	}

	if (ImGui::Button("Fill from Current Group") && tile_source_) {
		std::string group{ "Ungrouped" };
		for (const auto& entry : tile_library_) {
			PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
			if (candidate && candidate == tile_source_) {
				group = entry.group;
				break;
			}
		}

		std::size_t slot{};
		for (const auto& entry : tile_library_) {
			if (entry.group != group || slot >= active->tiles.size()) {
				continue;
			}

			PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
			if (candidate) {
				active->tiles[slot++] = candidate;
			}
		}
		SaveProjectLibrary(ctx);
	}

	ImGui::TextDisabled(
		"%s — %d variant slots", AutotileFormatName(active->format),
		static_cast<int>(active->tiles.size())
	);

	for (std::size_t i{}; i < active->tiles.size(); ++i) {
		ImGui::PushID(static_cast<int>(i));
		ImGui::Text("%02d", static_cast<int>(i));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (DrawTileSourceCombo(ctx, "##AutotileSlot", active->tiles[i])) {
			SaveProjectLibrary(ctx);
		}
		ImGui::PopID();
	}
}

void PaintEditor::EnsureLocalState(EditorContext& ctx) {
	const auto root{ ctx.editor.GetProjectRoot() };
	const std::string project{ root.has_value() ? root->lexically_normal().generic_string()
												: std::string{} };
	if (local_state_loaded_ && project == loaded_local_project_) {
		return;
	}

	local_state_loaded_	  = true;
	loaded_local_project_ = project;
	const PaintLocalState state{ ctx.local.paint.value_or(PaintLocalState{}) };
	tool_					= state.tool;
	recipe_					= state.recipe;
	brush_shape_			= state.brush_shape;
	selection_brush_shape_	= state.selection_brush_shape;
	select_mode_			= state.select_mode;
	area_mode_				= state.area_mode;
	move_snap_				= state.move_snap;
	brush_diameter_			= std::max(1, state.brush_diameter);
	selection_diameter_		= std::max(1, state.selection_diameter);
	line_thickness_			= std::max(1, state.line_thickness);
	line_spacing_			= std::max(1, state.line_spacing);
	area_thickness_			= std::max(1, state.area_thickness);
	line_align_rotation_	= state.line_align_rotation;
	grid_visible_			= state.grid_visible;
	entity_grid_size_		= { std::max(1.0f, state.entity_grid_size.x),
								std::max(1.0f, state.entity_grid_size.y) };
	entity_grid_offset_		= state.entity_grid_offset;
	grid_aspect_locked_		= state.grid_aspect_locked;
	grid_locked_aspect_		= std::max(0.001f, state.grid_locked_aspect);
	grid_major_every_		= std::max(1, state.grid_major_every);
	grid_minor_color_		= state.grid_minor_color;
	grid_major_color_		= state.grid_major_color;
	grid_minor_thickness_	= std::max(0.25f, state.grid_minor_thickness);
	grid_major_thickness_	= std::max(0.25f, state.grid_major_thickness);
	selected_tile_entry_id_ = state.selected_tile_entry_id;
	tile_source_			= state.tile_source;
	prefab_source_			= state.prefab_source;

	EnsureProjectLibrary(ctx);
}

void PaintEditor::StoreLocalState(EditorContext& ctx) const {
	if (!ctx.local.paint.has_value()) {
		ctx.local.paint.emplace();
	}
	auto& state{ *ctx.local.paint };
	state.tool					 = tool_;
	state.recipe				 = recipe_;
	state.brush_shape			 = brush_shape_;
	state.selection_brush_shape	 = selection_brush_shape_;
	state.select_mode			 = select_mode_;
	state.area_mode				 = area_mode_;
	state.move_snap				 = move_snap_;
	state.brush_diameter		 = brush_diameter_;
	state.selection_diameter	 = selection_diameter_;
	state.line_thickness		 = line_thickness_;
	state.line_spacing			 = line_spacing_;
	state.area_thickness		 = area_thickness_;
	state.line_align_rotation	 = line_align_rotation_;
	state.grid_visible			 = grid_visible_;
	state.entity_grid_size		 = entity_grid_size_;
	state.entity_grid_offset	 = entity_grid_offset_;
	state.grid_aspect_locked	 = grid_aspect_locked_;
	state.grid_locked_aspect	 = grid_locked_aspect_;
	state.grid_major_every		 = grid_major_every_;
	state.grid_minor_color		 = grid_minor_color_;
	state.grid_major_color		 = grid_major_color_;
	state.grid_minor_thickness	 = grid_minor_thickness_;
	state.grid_major_thickness	 = grid_major_thickness_;
	state.selected_tile_entry_id = selected_tile_entry_id_;
	state.tile_source			 = tile_source_;
	state.prefab_source			 = prefab_source_;
}

void PaintEditor::EnsureProjectLibrary(EditorContext& ctx) {
	const auto root{ ctx.editor.GetProjectRoot() };
	const std::string project{ root.has_value() ? root->lexically_normal().generic_string()
												: std::string{} };
	if (project_library_loaded_ && project == loaded_project_library_) {
		return;
	}

	project_library_loaded_ = true;
	loaded_project_library_ = project;
	tile_slice_settings_.clear();
	tile_library_.clear();
	tile_groups_.clear();
	prefab_groups_.clear();
	prefab_group_by_key_.clear();
	weighted_tile_sets_.clear();
	weighted_prefab_sets_.clear();
	autotile_rule_sets_.clear();

	const PaintProjectState& state{ ctx.project_state.paint };
	tile_groups_		  = state.tile_groups;
	prefab_groups_		  = state.prefab_groups;
	weighted_tile_sets_	  = state.weighted_tile_sets;
	weighted_prefab_sets_ = state.weighted_prefab_sets;
	autotile_rule_sets_	  = state.autotile_rule_sets;
	for (const auto& stored : state.tile_slice_settings) {
		tile_slice_settings_.insert_or_assign(
			stored.texture.value, TileSliceSettings{
									  .tile_size = stored.tile_size,
									  .margin	 = stored.margin,
									  .spacing	 = stored.spacing,
								  }
		);
	}
	for (const auto& stored : state.tiles) {
		tile_library_.push_back(
			TileLibraryEntry{
				.id		 = stored.id,
				.name	 = stored.name,
				.texture = stored.texture,
				.slice	 = stored.slice,
				.group	 = stored.group,
			}
		);
	}
	for (const auto& stored : state.prefab_group_assignments) {
		prefab_group_by_key_.insert_or_assign(stored.prefab.value, stored.group);
	}

	ReconcileProjectLibrary(ctx);

	if (!selected_tile_entry_id_.empty()) {
		if (const TileLibraryEntry* selected{ FindTileEntry(selected_tile_entry_id_) }) {
			inspected_texture_ = selected->texture;
			inspected_slice_   = selected->slice;
		}
	}
}

void PaintEditor::SaveProjectLibrary(EditorContext& ctx) const {
	PaintProjectState state;
	state.tile_groups	= tile_groups_;
	state.prefab_groups = prefab_groups_;
	state.tile_slice_settings.reserve(tile_slice_settings_.size());
	for (const auto& [texture, settings] : tile_slice_settings_) {
		state.tile_slice_settings.push_back(
			PaintTileSliceSettingsState{
				.texture   = TextureKey{ texture },
				.tile_size = settings.tile_size,
				.margin	   = settings.margin,
				.spacing   = settings.spacing,
			}
		);
	}
	std::ranges::sort(state.tile_slice_settings, {}, [](const PaintTileSliceSettingsState& item) {
		return item.texture.value;
	});

	state.tiles.reserve(tile_library_.size());
	for (const auto& entry : tile_library_) {
		state.tiles.push_back(
			PaintTileLibraryEntryState{
				.id		 = entry.id,
				.name	 = entry.name,
				.texture = entry.texture,
				.slice	 = entry.slice,
				.group	 = entry.group,
			}
		);
	}

	state.weighted_tile_sets   = weighted_tile_sets_;
	state.weighted_prefab_sets = weighted_prefab_sets_;
	state.autotile_rule_sets   = autotile_rule_sets_;

	state.prefab_group_assignments.reserve(prefab_group_by_key_.size());
	for (const auto& [prefab, group] : prefab_group_by_key_) {
		state.prefab_group_assignments.push_back(
			PaintPrefabGroupAssignmentState{
				.prefab = PrefabKey{ prefab },
				.group	= group,
			}
		);
	}
	std::ranges::sort(
		state.prefab_group_assignments, {},
		[](const PaintPrefabGroupAssignmentState& item) { return item.prefab.value; }
	);

	ctx.project_state.paint = std::move(state);
}

void PaintEditor::ReconcileProjectLibrary(EditorContext& ctx) {
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::unordered_set<std::string> texture_keys;
	for (const auto& record : records) {
		if (record.kind != AssetKind::Texture || record.engine_asset) {
			continue;
		}
		texture_keys.insert(record.key.value);
		TextureKey texture{ record.key };
		auto [settings_it, inserted]{ tile_slice_settings_.try_emplace(texture.value) };
		if (inserted && record.metadata.dimensions.has_value() &&
			record.metadata.dimensions->IsPositive()) {
			settings_it->second.tile_size = *record.metadata.dimensions;
		}
		const bool any_entry{ std::ranges::any_of(
			tile_library_, [&](const TileLibraryEntry& entry) { return entry.texture == texture; }
		) };
		if (!any_entry) {
			tile_library_.push_back(
				TileLibraryEntry{ .id	   = TileEntryId(texture, {}),
								  .name	   = TextureDisplayName(texture),
								  .texture = texture,
								  .slice   = {},
								  .group   = "Ungrouped" }
			);
		}
	}
	std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
		return !texture_keys.contains(entry.texture.value);
	});

	std::unordered_set<std::string> prefab_keys;
	for (const PrefabKey& key : ctx.editor.GetAssetManager().GetPrefabKeys()) {
		prefab_keys.insert(key.value);
	}
	std::erase_if(prefab_group_by_key_, [&](const auto& entry) {
		return !prefab_keys.contains(entry.first);
	});

	for (auto& set : weighted_tile_sets_) {
		std::erase_if(set.entries, [&](const PaintWeightedTileEntry& entry) {
			return !entry.source || !texture_keys.contains(entry.source.texture.value);
		});
	}
	for (auto& set : weighted_prefab_sets_) {
		std::erase_if(set.entries, [&](const PaintWeightedPrefabEntry& entry) {
			return !entry.prefab || !prefab_keys.contains(entry.prefab.value);
		});
	}
	std::unordered_set<std::uint64_t> autotile_ids;
	std::uint64_t next_autotile_id{ 1 };
	for (const auto& rules : autotile_rule_sets_) {
		next_autotile_id = std::max(next_autotile_id, rules.id + 1);
	}
	for (auto& rules : autotile_rule_sets_) {
		if (rules.id == 0 || !autotile_ids.insert(rules.id).second) {
			rules.id = next_autotile_id++;
			autotile_ids.insert(rules.id);
		}
		const std::size_t required{
			static_cast<std::size_t>(RequiredAutotileTileCount(rules.format))
		};
		rules.tiles.resize(required);
		for (auto& source : rules.tiles) {
			if (source.has_value() && !texture_keys.contains(source->texture.value)) {
				source.reset();
			}
		}
	}
	if (prefab_source_ && !prefab_keys.contains(prefab_source_.value)) {
		prefab_source_ = {};
	}
	if (tile_source_ && !texture_keys.contains(tile_source_.texture.value)) {
		tile_source_ = {};
	}
	if (!recipe_.weighted_tile_set_name.empty() &&
		!FindWeightedTileSet(recipe_.weighted_tile_set_name)) {
		recipe_.weighted_tile_set_name.clear();
	}
	if (!recipe_.weighted_prefab_set_name.empty() &&
		!FindWeightedPrefabSet(recipe_.weighted_prefab_set_name)) {
		recipe_.weighted_prefab_set_name.clear();
	}
	if (!recipe_.autotile_ruleset_name.empty() &&
		!FindAutotileRuleSet(recipe_.autotile_ruleset_name)) {
		recipe_.autotile_ruleset_name.clear();
	}

	std::erase_if(tile_groups_, [](const std::string& name) {
		return name.empty() || name == "Ungrouped";
	});
	std::erase_if(prefab_groups_, [](const std::string& name) {
		return name.empty() || name == "Ungrouped";
	});
	std::ranges::sort(tile_groups_);
	tile_groups_.erase(std::unique(tile_groups_.begin(), tile_groups_.end()), tile_groups_.end());
	std::ranges::sort(prefab_groups_);
	prefab_groups_.erase(
		std::unique(prefab_groups_.begin(), prefab_groups_.end()), prefab_groups_.end()
	);
}

std::vector<std::string> PaintEditor::TileGroupNames() const {
	std::vector<std::string> groups{ "Ungrouped" };
	groups.insert(groups.end(), tile_groups_.begin(), tile_groups_.end());
	for (const auto& entry : tile_library_) {
		if (entry.group != "Ungrouped" && !std::ranges::contains(groups, entry.group)) {
			groups.emplace_back(entry.group);
		}
	}
	if (groups.size() > 1) {
		std::ranges::sort(groups.begin() + 1, groups.end());
	}
	return groups;
}

std::vector<std::string> PaintEditor::GetPrefabGroups(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);
	std::vector<std::string> groups{ "Ungrouped" };
	groups.insert(groups.end(), prefab_groups_.begin(), prefab_groups_.end());
	for (const auto& [_, group] : prefab_group_by_key_) {
		if (group != "Ungrouped" && !std::ranges::contains(groups, group)) {
			groups.emplace_back(group);
		}
	}
	if (groups.size() > 1) {
		std::ranges::sort(groups.begin() + 1, groups.end());
	}
	return groups;
}

std::string PaintEditor::GetPrefabGroup(EditorContext& ctx, const PrefabKey& key) {
	EnsureProjectLibrary(ctx);
	const auto it{ prefab_group_by_key_.find(key.value) };
	return it == prefab_group_by_key_.end() || it->second.empty() ? "Ungrouped" : it->second;
}

bool PaintEditor::CreatePrefabGroup(EditorContext& ctx, std::string name) {
	EnsureProjectLibrary(ctx);
	name = Trim(std::move(name));
	if (name.empty() || name == "Ungrouped" || std::ranges::contains(prefab_groups_, name)) {
		return false;
	}
	prefab_groups_.emplace_back(std::move(name));
	std::ranges::sort(prefab_groups_);
	SaveProjectLibrary(ctx);
	return true;
}

bool PaintEditor::RenamePrefabGroup(
	EditorContext& ctx, std::string_view old_name, std::string new_name
) {
	EnsureProjectLibrary(ctx);
	new_name = Trim(std::move(new_name));
	if (old_name == "Ungrouped" || new_name.empty() || new_name == "Ungrouped") {
		return false;
	}
	if (std::ranges::contains(prefab_groups_, new_name) && new_name != old_name) {
		return false;
	}
	for (auto& name : prefab_groups_) {
		if (name == old_name) {
			name = new_name;
		}
	}
	for (auto& [_, group] : prefab_group_by_key_) {
		if (group == old_name) {
			group = new_name;
		}
	}
	std::ranges::sort(prefab_groups_);
	SaveProjectLibrary(ctx);
	return true;
}

bool PaintEditor::DeletePrefabGroup(EditorContext& ctx, std::string_view name) {
	EnsureProjectLibrary(ctx);
	if (name == "Ungrouped") {
		return false;
	}
	std::erase_if(prefab_groups_, [&](const std::string& group) { return group == name; });
	for (auto& [_, group] : prefab_group_by_key_) {
		if (group == name) {
			group = "Ungrouped";
		}
	}
	SaveProjectLibrary(ctx);
	return true;
}

void PaintEditor::MovePrefabToGroup(EditorContext& ctx, const PrefabKey& key, std::string group) {
	EnsureProjectLibrary(ctx);
	if (group.empty()) {
		group = "Ungrouped";
	}
	prefab_group_by_key_.insert_or_assign(key.value, std::move(group));
	SaveProjectLibrary(ctx);
}

void PaintEditor::OnPrefabRenamed(
	EditorContext& ctx, const PrefabKey& old_key, const PrefabKey& new_key
) {
	EnsureProjectLibrary(ctx);
	bool project_changed{};
	if (const auto it{ prefab_group_by_key_.find(old_key.value) };
		it != prefab_group_by_key_.end()) {
		prefab_group_by_key_[new_key.value] = it->second;
		prefab_group_by_key_.erase(it);
		project_changed = true;
	}
	if (prefab_source_ == old_key) {
		prefab_source_ = new_key;
	}
	if (recipe_.checker_prefab == old_key) {
		recipe_.checker_prefab = new_key;
	}
	for (auto& threshold : recipe_.noise.thresholds) {
		if (threshold.prefab == old_key) {
			threshold.prefab = new_key;
		}
	}
	for (auto& set : weighted_prefab_sets_) {
		for (auto& entry : set.entries) {
			if (entry.prefab == old_key) {
				entry.prefab	= new_key;
				project_changed = true;
			}
		}
	}
	if (project_changed) {
		SaveProjectLibrary(ctx);
	}
}

void PaintEditor::OnPrefabDuplicated(
	EditorContext& ctx, const PrefabKey& source, const PrefabKey& duplicate
) {
	MovePrefabToGroup(ctx, duplicate, GetPrefabGroup(ctx, source));
}

void PaintEditor::OnPrefabDeleted(EditorContext& ctx, const PrefabKey& key) {
	EnsureProjectLibrary(ctx);
	bool project_changed{ prefab_group_by_key_.erase(key.value) != 0 };
	if (prefab_source_ == key) {
		prefab_source_ = {};
	}
	if (recipe_.checker_prefab == key) {
		recipe_.checker_prefab.reset();
	}
	for (auto& threshold : recipe_.noise.thresholds) {
		if (threshold.prefab == key) {
			threshold.prefab.reset();
		}
	}
	for (auto& set : weighted_prefab_sets_) {
		const auto before{ set.entries.size() };
		std::erase_if(set.entries, [&](const PaintWeightedPrefabEntry& entry) {
			return entry.prefab == key;
		});
		project_changed |= before != set.entries.size();
	}
	if (project_changed) {
		SaveProjectLibrary(ctx);
	}
}

int PaintEditor::ImportImageTiles(
	EditorContext& ctx, const TileImportSettings& settings, const std::string& image_path_text,
	std::string group
) {
	const std::filesystem::path image_path{ image_path_text };
	if (!std::filesystem::exists(image_path) || !IsImagePath(image_path)) {
		return 0;
	}
	auto& assets{ ctx.editor.GetAssetManager() };
	const auto imported{ assets.ImportAsset(image_path) };
	if (!imported.has_value()) {
		return 0;
	}
	const TextureKey texture{ imported->value };
	if (const auto catalog{ assets.GetCatalogAsset(*imported, AssetKind::Texture) };
		catalog.has_value()) {
		assets.Load(*imported, catalog->source_path);
	}
	V2_int texture_size{};
	for (const auto& record : ::ptgn::impl::AssetAccessor{ assets }.GetAssets()) {
		if (record.kind == AssetKind::Texture && record.key == *imported &&
			record.metadata.dimensions.has_value()) {
			texture_size = *record.metadata.dimensions;
			break;
		}
	}
	if (!texture_size.IsPositive() && assets.Has(texture)) {
		texture_size = assets.GetTextureSize(texture);
	}
	if (!texture_size.IsPositive()) {
		return 0;
	}

	std::optional<V2_int> inferred;
	if (settings.use_filename_dimensions) {
		inferred = FilenameTileDimensions(image_path);
	}
	bool slice{};
	V2_int tile_size{ texture_size };
	if (settings.mode == TileImportMode::Individual) {
		slice = false;
	} else if (settings.mode == TileImportMode::Tileset) {
		slice	  = true;
		tile_size = inferred.value_or(
			V2_int{ std::max(1, settings.tile_width), std::max(1, settings.tile_height) }
		);
	} else if (inferred.has_value()) {
		slice	  = true;
		tile_size = *inferred;
	}

	if (settings.create_group_from_source) {
		group = SourceGroupName(image_path);
		if (group != "Ungrouped" && !std::ranges::contains(tile_groups_, group)) {
			tile_groups_.emplace_back(group);
		}
	}
	if (group.empty()) {
		group = "Ungrouped";
	}

	TileSliceSettings slicing{ .tile_size = slice ? tile_size : texture_size,
							   .margin	  = slice ? V2_int{ std::max(0, settings.margin_x),
															std::max(0, settings.margin_y) }
												  : V2_int{},
							   .spacing	  = slice ? V2_int{ std::max(0, settings.spacing_x),
															std::max(0, settings.spacing_y) }
												  : V2_int{} };
	tile_slice_settings_.insert_or_assign(texture.value, slicing);
	std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
		return entry.texture == texture;
	});

	const int step_x{ std::max(1, slicing.tile_size.x + slicing.spacing.x) };
	const int step_y{ std::max(1, slicing.tile_size.y + slicing.spacing.y) };
	const int columns{
		slice ? std::max(0, (texture_size.x - slicing.margin.x + slicing.spacing.x) / step_x) : 1
	};
	const int rows{
		slice ? std::max(0, (texture_size.y - slicing.margin.y + slicing.spacing.y) / step_y) : 1
	};
	int count{};
	for (int y{}; y < rows; ++y) {
		for (int x{}; x < columns; ++x) {
			const V2_int coord{ x, y };
			const std::string name{ slice ? image_path.stem().string() + "_" + std::to_string(count)
										  : image_path.stem().string() };
			tile_library_.push_back(
				TileLibraryEntry{ .id	   = TileEntryId(texture, coord),
								  .name	   = name,
								  .texture = texture,
								  .slice   = coord,
								  .group   = group }
			);
			++count;
		}
	}
	if (count > 0) {
		selected_tile_entry_id_ = tile_library_.back().id;
		inspected_texture_		= texture;
		inspected_slice_		= tile_library_.back().slice;
		SaveProjectLibrary(ctx);
	}
	return count;
}

int PaintEditor::ImportTiles(EditorContext& ctx, const TileImportSettings& settings) {
	if (settings.path.empty()) {
		return 0;
	}
	const std::filesystem::path source{ settings.path };
	if (!std::filesystem::exists(source)) {
		return 0;
	}
	std::string ext{ source.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	std::string group{ settings.target_group.empty() ? "Ungrouped" : settings.target_group };
	if (ext == ".tsx" || ext == ".tsj" || ext == ".json") {
		const TiledTilesetInfo info{ ParseTiledTileset(source) };
		int total{};
		for (const auto& image_name : info.images) {
			std::filesystem::path image{ image_name };
			if (image.is_relative()) {
				image = source.parent_path() / image;
			}
			TileImportSettings child{ settings };
			child.create_group_from_source = false;
			if (info.images.size() > 1) {
				child.mode = TileImportMode::Individual;
			} else {
				child.mode					  = TileImportMode::Tileset;
				child.use_filename_dimensions = false;
				child.tile_width  = info.tile_width > 0 ? info.tile_width : settings.tile_width;
				child.tile_height = info.tile_height > 0 ? info.tile_height : settings.tile_height;
				child.margin_x = child.margin_y = info.margin;
				child.spacing_x = child.spacing_y = info.spacing;
			}
			total += ImportImageTiles(
				ctx, child, image.lexically_normal().string(),
				settings.create_group_from_source ? SourceGroupName(source) : group
			);
		}
		return total;
	}
	return ImportImageTiles(ctx, settings, source.lexically_normal().string(), group);
}

void PaintEditor::DrawImportPopup(EditorContext& ctx) {
	if (import_popup_requested_) {
		ImGui::OpenPopup("Import Tiles");
		import_popup_requested_ = false;
	}
	if (!ImGui::BeginPopupModal("Import Tiles", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}
	ImGui::InputText("Source", &import_settings_.path);
	ImGui::SameLine();
	if (ImGui::Button("Browse...")) {
		auto result{ ctx.editor.GetWindow().file.OpenFiles(TileImportOptions()) };
		if (!result.has_value()) {
			import_status_ = "File picker failed: " + result.error();
		} else if (result->has_value() && !result->value().empty()) {
			import_settings_.path = result->value().front().string();
		}
	}
	int mode{ static_cast<int>(import_settings_.mode) };
	if (ImGui::Combo("Mode", &mode, "Auto\0Tileset\0Individual\0")) {
		import_settings_.mode = static_cast<TileImportMode>(mode);
	}
	if (import_settings_.mode != TileImportMode::Individual) {
		ImGui::DragInt("Tile Width", &import_settings_.tile_width, 1.0f, 1, 8192);
		ImGui::DragInt("Tile Height", &import_settings_.tile_height, 1.0f, 1, 8192);
		int margin[2]{ import_settings_.margin_x, import_settings_.margin_y };
		if (ImGui::DragInt2("Margin", margin, 1.0f, 0, 8192)) {
			import_settings_.margin_x = margin[0];
			import_settings_.margin_y = margin[1];
		}
		int spacing[2]{ import_settings_.spacing_x, import_settings_.spacing_y };
		if (ImGui::DragInt2("Spacing", spacing, 1.0f, 0, 8192)) {
			import_settings_.spacing_x = spacing[0];
			import_settings_.spacing_y = spacing[1];
		}
		ImGui::Checkbox(
			"Use _32x32 / -32x32 filename dimensions", &import_settings_.use_filename_dimensions
		);
	}
	ImGui::Checkbox("Create group from source name", &import_settings_.create_group_from_source);
	if (!import_settings_.create_group_from_source) {
		auto groups{ TileGroupNames() };
		if (ImGui::BeginCombo("Group", import_settings_.target_group.c_str())) {
			for (const auto& group : groups) {
				if (ImGui::Selectable(group.c_str(), group == import_settings_.target_group)) {
					import_settings_.target_group = group;
				}
			}
			ImGui::EndCombo();
		}
	}
	if (!import_status_.empty()) {
		ImGui::TextDisabled("%s", import_status_.c_str());
	}
	if (ImGui::Button("Import")) {
		const int count{ ImportTiles(ctx, import_settings_) };
		import_status_ =
			count > 0 ? "Imported " + std::to_string(count) + " tile" + (count == 1 ? "." : "s.")
					  : "Nothing was imported.";
		if (count > 0) {
			ImGui::CloseCurrentPopup();
		}
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel")) {
		ImGui::CloseCurrentPopup();
	}
	ImGui::EndPopup();
}

bool PaintEditor::DrawTilesPanel(EditorContext& ctx) {
	EnsureLocalState(ctx);
	const bool visible{ ImGui::Begin("Tiles###TilesWindow") };
	if (visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows)) {
		ctx.editor.GetSceneHierarchyPanel().SetActiveTab(SceneHierarchyTab::Tiles);
	}
	if (!visible) {
		ImGui::End();
		return false;
	}
	EnsureProjectLibrary(ctx);

	if (ImGui::Button("+ Group")) {
		auto groups{ TileGroupNames() };
		tile_groups_.push_back(NextDefaultGroupName(groups));
		std::ranges::sort(tile_groups_);
		SaveProjectLibrary(ctx);
	}
	ImGui::SameLine();
	if (ImGui::Button("Import...")) {
		import_settings_.target_group = "Ungrouped";
		import_popup_requested_		  = true;
	}
	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##TileSearch", "Filter tile or group...", &tile_search_);

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::optional<std::pair<std::string, std::string>> pending_group_move;
	for (const std::string& group : TileGroupNames()) {
		const bool group_matches{ ContainsInsensitive(group, tile_search_) };
		const bool has_matching{ std::ranges::any_of(tile_library_, [&](const TileLibraryEntry& e) {
			return e.group == group &&
				   (group_matches || ContainsInsensitive(e.name, tile_search_) ||
					ContainsInsensitive(e.texture.value, tile_search_));
		}) };
		if (!has_matching && !tile_search_.empty()) {
			continue;
		}
		ImGui::PushID(group.c_str());
		const bool open{ ImGui::TreeNodeEx(
			"##TileGroup", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
			group.c_str()
		) };
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload{
					ImGui::AcceptDragDropPayload(kTileLibraryEntryDragDropPayload) };
				payload && payload->Data && payload->DataSize > 1) {
				pending_group_move = std::pair{
					std::string{ static_cast<const char*>(payload->Data) },
					group,
				};
			}
			ImGui::EndDragDropTarget();
		}
		static std::string rename_group;
		if (ImGui::BeginPopupContextItem("TileGroupContext")) {
			if (group != "Ungrouped") {
				if (ImGui::MenuItem("Rename Group")) {
					rename_group = group;
					ImGui::OpenPopup("Rename Tile Group");
				}
				if (ImGui::MenuItem("Delete Group")) {
					for (auto& e : tile_library_) {
						if (e.group == group) {
							e.group = "Ungrouped";
						}
					}
					std::erase(tile_groups_, group);
					SaveProjectLibrary(ctx);
				}
			}
			if (ImGui::MenuItem("Import into Group...")) {
				import_settings_.target_group			  = group;
				import_settings_.create_group_from_source = false;
				import_popup_requested_					  = true;
			}
			ImGui::EndPopup();
		}
		if (!rename_group.empty()) {
			ImGui::SetNextWindowSize({ 340, 0 }, ImGuiCond_Appearing);
			if (ImGui::BeginPopupModal(
					"Rename Tile Group", nullptr, ImGuiWindowFlags_AlwaysAutoResize
				)) {
				static std::string new_name;
				if (ImGui::IsWindowAppearing()) {
					new_name = rename_group;
				}
				ImGui::InputText("Name", &new_name);
				if (ImGui::Button("Rename")) {
					new_name = Trim(new_name);
					if (!new_name.empty() && new_name != "Ungrouped" &&
						(!std::ranges::contains(tile_groups_, new_name) ||
						 new_name == rename_group)) {
						for (auto& e : tile_library_) {
							if (e.group == rename_group) {
								e.group = new_name;
							}
						}
						for (auto& name : tile_groups_) {
							if (name == rename_group) {
								name = new_name;
							}
						}
						std::ranges::sort(tile_groups_);
						SaveProjectLibrary(ctx);
						rename_group.clear();
						ImGui::CloseCurrentPopup();
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) {
					rename_group.clear();
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
		}

		if (open) {
			int shown{};
			const float cell{ 48.0f };
			const int per_row{
				std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (cell + 5.0f)))
			};
			std::optional<std::string> delete_id;
			for (auto& entry : tile_library_) {
				if (entry.group != group ||
					(!group_matches && !ContainsInsensitive(entry.name, tile_search_) &&
					 !ContainsInsensitive(entry.texture.value, tile_search_))) {
					continue;
				}
				if (shown % per_row != 0) {
					ImGui::SameLine();
				}
				ImGui::PushID(entry.id.c_str());
				const PaintTileSource source{ MakeTileSource(ctx, entry.texture, entry.slice) };
				const ImVec2 p0{ ImGui::GetCursorScreenPos() };
				ImGui::InvisibleButton("##Tile", { cell, cell });
				const ImVec2 p1{ p0.x + cell, p0.y + cell };
				auto* dl{ ImGui::GetWindowDrawList() };
				dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
				const auto record_it{ std::ranges::find_if(records, [&](const auto& r) {
					return r.kind == AssetKind::Texture &&
						   r.key == static_cast<const AssetKey&>(entry.texture);
				}) };
				if (source && record_it != records.end() && record_it->preview.has_value()) {
					const auto& uv{ source.texture_coordinates };
					dl->AddImageQuad(
						static_cast<ImTextureID>(record_it->preview->texture), p0, { p1.x, p0.y },
						p1, { p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]),
						ToImGui(uv[3])
					);
				}
				if (selected_tile_entry_id_ == entry.id) {
					dl->AddRect(p0, p1, kSelection, 3.0f, 0, 2.0f);
				}
				const bool tile_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };
				const bool tile_hovered{ ImGui::IsItemHovered() };
				if (tile_clicked) {
					selected_tile_entry_id_ = entry.id;
					inspected_texture_		= entry.texture;
					inspected_slice_		= entry.slice;
				}
				if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
					ImGui::OpenPopup("TileContext");
				}
				if (ImGui::BeginDragDropSource()) {
					ImGui::SetDragDropPayload(
						kTileLibraryEntryDragDropPayload, entry.id.c_str(), entry.id.size() + 1
					);
					ImGui::TextUnformatted(entry.name.c_str());
					ImGui::EndDragDropSource();
				}
				if (tile_hovered) {
					V2_int dimensions{};
					if (source) {
						dimensions = source.pixel_size;
					} else if (const auto* settings{ FindSliceSettings(entry.texture) }) {
						dimensions = settings->tile_size;
					}
					if (dimensions.IsPositive()) {
						ImGui::SetTooltip(
							"%s\n%dx%d", entry.name.c_str(), dimensions.x, dimensions.y
						);
					} else {
						ImGui::SetTooltip("%s\nUnknown dimensions", entry.name.c_str());
					}
				}
				if (ImGui::BeginPopup("TileContext")) {
					if (ImGui::BeginMenu("Move to Group")) {
						for (const auto& target : TileGroupNames()) {
							if (ImGui::MenuItem(target.c_str(), nullptr, target == entry.group)) {
								entry.group = target;
								SaveProjectLibrary(ctx);
							}
						}
						ImGui::EndMenu();
					}
					if (ImGui::MenuItem("Remove from Tile Library")) {
						delete_id = entry.id;
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
				++shown;
			}
			if (delete_id.has_value()) {
				std::erase_if(tile_library_, [&](const TileLibraryEntry& e) {
					return e.id == *delete_id;
				});
				if (selected_tile_entry_id_ == *delete_id) {
					selected_tile_entry_id_.clear();
					inspected_texture_.reset();
				}
				SaveProjectLibrary(ctx);
			}
			if (shown == 0) {
				ImGui::TextDisabled("Empty group. Right click the group to import tiles.");
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}
	if (pending_group_move.has_value()) {
		if (TileLibraryEntry* entry{ FindTileEntry(pending_group_move->first) };
			entry && entry->group != pending_group_move->second) {
			entry->group = pending_group_move->second;
			SaveProjectLibrary(ctx);
		}
	}
	DrawImportPopup(ctx);
	if (!import_status_.empty()) {
		ImGui::TextDisabled("%s", import_status_.c_str());
	}
	StoreLocalState(ctx);
	ImGui::End();
	return true;
}

void PaintEditor::BakeGenerator(EditorContext& ctx, Scene& scene, Entity entity) {
	if (!entity || !IsPaintGenerator(entity)) {
		return;
	}

	PaintGenerator generator{ entity };
	const auto& data{ generator.GetData() };
	if (data.geometry == PaintGeneratorGeometry::Infinite) {
		return;
	}

	const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
	if (!layer_id.has_value()) {
		return;
	}
	SceneLayer* layer{ scene.GetLayers().Find(*layer_id) };
	if (!layer || layer->locked) {
		return;
	}

	const SerializedEntity generator_snapshot{ SerializeEntity(entity) };
	const V2_float entity_offset{ GetWorldPosition(generator) };
	const V2_float origin{ data.grid_offset + entity_offset };
	auto to_cell = [&](V2_float world) {
		return V2_int{
			static_cast<int>(std::floor((world.x - origin.x) / std::max(1.0f, data.grid_size.x))),
			static_cast<int>(std::floor((world.y - origin.y) / std::max(1.0f, data.grid_size.y))),
		};
	};
	auto cell_world = [&](V2_int cell) {
		return origin + V2_float{
			static_cast<float>(cell.x),
			static_cast<float>(cell.y),
		} * data.grid_size;
	};

	std::unordered_set<V2_int> cells;
	auto line_cells = [](V2_int a, V2_int b, int thickness, int spacing) {
		std::vector<V2_int> result;
		const int dx{ std::abs(b.x - a.x) };
		const int dy{ -std::abs(b.y - a.y) };
		const int sx{ a.x < b.x ? 1 : -1 };
		const int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};

		for (;;) {
			if (step % std::max(1, spacing) == 0) {
				const int half{ std::max(1, thickness) / 2 };
				for (int y{ -half }; y <= half; ++y) {
					for (int x{ -half }; x <= half; ++x) {
						result.emplace_back(a + V2_int{ x, y });
					}
				}
			}

			if (a == b) {
				break;
			}
			const int e2{ 2 * error };
			if (e2 >= dy) {
				error += dy;
				a.x	  += sx;
			}
			if (e2 <= dx) {
				error += dx;
				a.y	  += sy;
			}
			++step;
		}
		return result;
	};

	switch (data.geometry) {
		case PaintGeneratorGeometry::Line:
			for (V2_int cell : line_cells(
					 to_cell(data.start + entity_offset), to_cell(data.end + entity_offset),
					 data.line_thickness, data.line_spacing
				 )) {
				cells.insert(cell);
			}
			break;

		case PaintGeneratorGeometry::Rectangle: {
			const V2_int a{ to_cell(data.start + entity_offset) };
			const V2_int b{ to_cell(data.end + entity_offset) };
			const int min_x{ std::min(a.x, b.x) };
			const int max_x{ std::max(a.x, b.x) };
			const int min_y{ std::min(a.y, b.y) };
			const int max_y{ std::max(a.y, b.y) };

			for (int y{ min_y }; y <= max_y; ++y) {
				for (int x{ min_x }; x <= max_x; ++x) {
					const int left{ x - min_x };
					const int right{ max_x - x };
					const int top{ y - min_y };
					const int bottom{ max_y - y };
					bool include{};

					switch (data.area_mode) {
						case PaintGeneratorAreaMode::Fill: include = true; break;
						case PaintGeneratorAreaMode::RandomFill:
							include = Hash01({ x, y }) <= data.random_fill_density;
							break;
						case PaintGeneratorAreaMode::Outline:
							include = left < data.area_thickness || right < data.area_thickness ||
									  top < data.area_thickness || bottom < data.area_thickness;
							break;
						case PaintGeneratorAreaMode::Corners: {
							const int thickness{ std::max(1, data.area_thickness) };
							include = (left < thickness && top < thickness) ||
									  (right < thickness && top < thickness) ||
									  (left < thickness && bottom < thickness) ||
									  (right < thickness && bottom < thickness);
							break;
						}
					}
					if (include) {
						cells.insert({ x, y });
					}
				}
			}
			break;
		}

		case PaintGeneratorGeometry::BrushStroke:
			for (const auto& point : data.stroke_points) {
				const V2_int center{ to_cell(point.position + entity_offset) };
				const int diameter{ std::max(1, point.diameter) };
				const int low{ -(diameter / 2) };
				const int high{ low + diameter - 1 };
				const float radius{ static_cast<float>(diameter) * 0.5f };

				for (int y{ low }; y <= high; ++y) {
					for (int x{ low }; x <= high; ++x) {
						if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
							const float dx{ static_cast<float>(x) + 0.5f };
							const float dy{ static_cast<float>(y) + 0.5f };
							if (std::sqrt(dx * dx + dy * dy) > radius) {
								continue;
							}
						}
						cells.insert(center + V2_int{ x, y });
					}
				}
			}
			break;

		case PaintGeneratorGeometry::Infinite: return;
	}

	// Resolve generated source exactly from the generator's captured recipe. This
	// intentionally does not depend on the current paint recipe.
	auto resolve_source = [&](V2_int cell, V2_float world_center) {
		struct Result {
			std::optional<PaintGeneratorTileSource> tile{};
			std::optional<PrefabKey> prefab{};
			Origin entity_origin{ Origin::Center };
		};

		Result result{ .entity_origin = data.recipe.entity_origin };

		const float noise_value{ data.recipe.source_kind == PaintGeneratorSourceKind::Noise
									 ? FractalNoise01(
										   world_center, data.recipe.noise.type,
										   data.recipe.noise.seed, data.recipe.noise.frequency,
										   data.recipe.noise.octaves, data.recipe.noise.lacunarity,
										   data.recipe.noise.persistence, data.recipe.noise.offset
									   )
									 : 0.0f };

		switch (data.recipe.source_kind) {
			case PaintGeneratorSourceKind::Single:
				result.tile	  = data.recipe.tile;
				result.prefab = data.recipe.prefab;
				break;

			case PaintGeneratorSourceKind::WeightedSet:
				if (layer->kind == SceneLayerKind::Tile) {
					const auto i{ ChooseWeightedIndex(
						data.recipe.weighted_tiles, cell,
						[](const auto& entry) { return entry.weight; }
					) };
					if (i) {
						result.tile = data.recipe.weighted_tiles[*i].source;
					}
				} else {
					const auto i{ ChooseWeightedIndex(
						data.recipe.weighted_prefabs, cell,
						[](const auto& entry) { return entry.weight; }
					) };
					if (i) {
						result.prefab = data.recipe.weighted_prefabs[*i].prefab;
					}
				}
				break;

			case PaintGeneratorSourceKind::Checkerboard:
				if ((cell.x + cell.y) & 1) {
					result.tile	  = data.recipe.checker_tile;
					result.prefab = data.recipe.checker_prefab;
				} else {
					result.tile	  = data.recipe.tile;
					result.prefab = data.recipe.prefab;
				}
				break;

			case PaintGeneratorSourceKind::Autotile: {
				const int index{ AutotileIndex(
					static_cast<PaintAutotileFormat>(data.recipe.autotile_format),
					[&cells](V2_int candidate) { return cells.contains(candidate); }, cell
				) };
				if (index >= 0 && index < static_cast<int>(data.recipe.autotile_tiles.size())) {
					result.tile = data.recipe.autotile_tiles[static_cast<std::size_t>(index)];
				}
				break;
			}

			case PaintGeneratorSourceKind::Noise:
				for (const auto& region : data.recipe.noise.thresholds) {
					if (!region.enabled) {
						continue;
					}
					const float lo{ std::min(region.minimum, region.maximum) };
					const float hi{ std::max(region.minimum, region.maximum) };
					if (noise_value < lo ||
						(noise_value >= hi && !(hi >= 0.99999f && noise_value <= 1.0f))) {
						continue;
					}

					result.entity_origin = region.origin;
					if (region.source_kind == PaintGeneratorSourceKind::WeightedSet) {
						if (layer->kind == SceneLayerKind::Tile) {
							const auto i{ ChooseWeightedIndex(
								region.weighted_tiles, cell,
								[](const auto& entry) { return entry.weight; }
							) };
							if (i) {
								result.tile = region.weighted_tiles[*i].source;
							}
						} else {
							const auto i{ ChooseWeightedIndex(
								region.weighted_prefabs, cell,
								[](const auto& entry) { return entry.weight; }
							) };
							if (i) {
								result.prefab = region.weighted_prefabs[*i].prefab;
							}
						}
					} else {
						result.tile	  = region.tile;
						result.prefab = region.prefab;
					}
					break;
				}
				break;
		}
		return result;
	};

	std::optional<::ptgn::impl::TilemapData> tilemap_before;
	std::optional<::ptgn::impl::TilemapData> tilemap_after;
	std::optional<UUID> tilemap_uuid;
	std::vector<DeletedEntity> created_entities;

	if (layer->kind == SceneLayerKind::Tile) {
		SetActiveLayer(scene, *layer_id);
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}

		tilemap_uuid   = map.Get<UUID>();
		tilemap_before = map.GetData();

		for (V2_int cell : cells) {
			if (generator.IsSuppressed(cell) ||
				(data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity &&
				 Hash01(cell) > data.recipe.density)) {
				continue;
			}
			if (data.recipe.avoid_exclusion_mask && map.IsExcluded(cell)) {
				continue;
			}

			const V2_float mn{ cell_world(cell) };
			const V2_float center{ mn + data.grid_size * 0.5f };
			const auto source{ resolve_source(cell, center) };
			if (!source.tile.has_value()) {
				continue;
			}

			const auto& tile{ *source.tile };
			const V2_float visual_size{ tile.pixel_size.IsPositive() ? V2_float{ tile.pixel_size }
																	 : data.grid_size };
			if (data.recipe.tile_placement == PaintGeneratorTilePlacementMode::Tile) {
				const V2_int footprint{
					std::max(
						1, static_cast<int>(
							   std::ceil(visual_size.x / std::max(1.0f, data.grid_size.x))
						   )
					),
					std::max(
						1, static_cast<int>(
							   std::ceil(visual_size.y / std::max(1.0f, data.grid_size.y))
						   )
					),
				};
				const V2_int lattice_origin{ to_cell(data.start + entity_offset) };
				if (FloorMod(cell.x - lattice_origin.x, footprint.x) != 0 ||
					FloorMod(cell.y - lattice_origin.y, footprint.y) != 0) {
					continue;
				}
			}

			// Keep the same Paint semantics used while authoring: generated content
			// does not overwrite an already authored tile.
			if (map.FindTile(cell)) {
				continue;
			}

			map.SetTile(
				TilemapTile{
					.coordinate			 = cell,
					.texture			 = tile.texture,
					.texture_coordinates = tile.texture_coordinates,
					.pixel_size			 = tile.pixel_size,
					.origin				 = tile.origin,
					.offset				 = {},
					.tint				 = color::White,
					.terrain_ruleset_id	 = std::nullopt,
				}
			);
		}

		tilemap_after = map.GetData();
	} else {
		auto& assets{ ctx.editor.GetAssetManager() };

		for (V2_int cell : cells) {
			if (generator.IsSuppressed(cell) ||
				(data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity &&
				 Hash01(cell) > data.recipe.density)) {
				continue;
			}

			const V2_float center{ cell_world(cell) + data.grid_size * 0.5f };
			const auto source{ resolve_source(cell, center) };
			if (!source.prefab.has_value()) {
				continue;
			}

			const PrefabKey prefab_key{ *source.prefab };

			bool occupied{};
			for (Entity root : scene.GetLayers().GetRootEntities(scene, *layer_id)) {
				if (!root || root == entity || IsProtectedSceneEntity(scene, root) ||
					IsPaintGenerator(root) || !root.Has<Transform>()) {
					continue;
				}
				if (to_cell(GetWorldPosition(root)) == cell) {
					occupied = true;
					break;
				}
			}
			if (occupied) {
				continue;
			}

			if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(prefab_key)) {
				const auto catalog{ assets.GetCatalogAsset(prefab_key, AssetKind::Prefab) };
				if (!catalog.has_value()) {
					continue;
				}
				assets.Load(prefab_key, catalog->source_path);
			}
			if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(prefab_key)) {
				continue;
			}

			V2_float world{ cell_world(cell) + GetOffset(source.entity_origin, data.grid_size) };

			if (data.recipe.min_spacing > 0.0f) {
				bool too_close{};
				for (Entity root : scene.GetLayers().GetRootEntities(scene, *layer_id)) {
					if (!root || root == entity || !root.Has<Transform>() ||
						IsPaintGenerator(root) || IsProtectedSceneEntity(scene, root)) {
						continue;
					}
					const V2_float delta{ GetWorldPosition(root) - world };
					if (std::sqrt(delta.x * delta.x + delta.y * delta.y) <
						data.recipe.min_spacing) {
						too_close = true;
						break;
					}
				}
				if (too_close) {
					continue;
				}
			}

			auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_key) };
			Entity created{ InstantiatePrefab(
				scene, prefab_asset.get(),
				data.recipe.link_prefab_instances ? PrefabInstantiationMode::Linked
												  : PrefabInstantiationMode::Baked
			) };
			if (!created) {
				continue;
			}
			if (!scene.GetLayers().Assign(created, *layer_id, true)) {
				created.Destroy();
				scene.Refresh();
				continue;
			}

			Transform transform{ GetWorldTransform(created) };
			transform.position = world;
			SetWorldTransform(created, transform);

			const float selector{ Hash01(cell) };
			if (data.recipe.random_rotation) {
				SetRotation(
					created,
					Degrees{ data.recipe.rotation_min +
							 (data.recipe.rotation_max - data.recipe.rotation_min) * selector }
				);
			}
			if (data.recipe.random_scale) {
				SetScale(
					created, data.recipe.scale_min +
								 (data.recipe.scale_max - data.recipe.scale_min) * Hash01(
																					   {
																						   cell.y,
																						   cell.x,
																					   }
																				   )
				);
			}

			created_entities.push_back(
				DeletedEntity{
					.entity = SerializeEntity(created),
					.layer	= *layer_id,
				}
			);
		}
	}

	const UUID generator_uuid{ entity.Get<UUID>() };
	entity.Destroy();
	scene.Refresh();

	if (active_brush_generator_.has_value() && *active_brush_generator_ == generator_uuid) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		next_active_brush_stroke_id_ = 1;
	}

	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);

	Scene* scene_ptr{ &scene };
	const SceneLayerId baked_layer{ *layer_id };

	auto restore_generator = [scene_ptr, generator_snapshot, baked_layer]() {
		if (generator_snapshot.uuid.has_value() && scene_ptr->GetEntity(*generator_snapshot.uuid)) {
			return;
		}
		(void)RestoreEntityTree(*scene_ptr, generator_snapshot, baked_layer);
		scene_ptr->Refresh();
	};

	auto remove_generator = [scene_ptr, generator_uuid]() {
		if (Entity generator{ scene_ptr->GetEntity(generator_uuid) }) {
			generator.Destroy();
			scene_ptr->Refresh();
		}
	};

	if (tilemap_uuid.has_value() && tilemap_before.has_value() && tilemap_after.has_value()) {
		const UUID map_uuid{ *tilemap_uuid };
		const auto before{ *tilemap_before };
		const auto after{ *tilemap_after };

		ctx.undo.PushApplied(
			"Bake Generator",
			[scene_ptr, map_uuid, before, restore_generator]() mutable {
				if (Entity map{ scene_ptr->GetEntity(map_uuid) };
					map && map.Has<::ptgn::impl::TilemapData>()) {
					map.Get<::ptgn::impl::TilemapData>() = before;
				}
				restore_generator();
			},
			[scene_ptr, map_uuid, after, remove_generator]() mutable {
				remove_generator();
				if (Entity map{ scene_ptr->GetEntity(map_uuid) };
					map && map.Has<::ptgn::impl::TilemapData>()) {
					map.Get<::ptgn::impl::TilemapData>() = after;
				}
			}
		);
	} else {
		auto apply_entities = [scene_ptr](
								  const std::vector<DeletedEntity>& remove,
								  const std::vector<DeletedEntity>& restore
							  ) {
			for (const auto& item : remove) {
				if (!item.entity.uuid.has_value()) {
					continue;
				}
				if (Entity root{ scene_ptr->GetEntity(*item.entity.uuid) }) {
					root.Destroy();
				}
			}
			scene_ptr->Refresh();

			for (const auto& item : restore) {
				(void)RestoreEntityTree(*scene_ptr, item.entity, item.layer);
			}
			scene_ptr->Refresh();
		};

		ctx.undo.PushApplied(
			"Bake Generator",
			[apply_entities, created_entities, restore_generator]() mutable {
				apply_entities(created_entities, {});
				restore_generator();
			},
			[apply_entities, created_entities, remove_generator]() mutable {
				remove_generator();
				apply_entities({}, created_entities);
			}
		);
	}

	ctx.local.state.is_dirty = true;
}

void PaintEditor::DrawGeneratorInspector(EditorContext& ctx, Entity entity) {
	if (!entity || !IsPaintGenerator(entity)) {
		ImGui::TextDisabled("Select a generator to inspect it.");
		return;
	}

	Scene& scene{ entity.GetScene() };
	PaintGenerator generator{ entity };
	auto& data{ generator.GetData() };

	auto begin_edit = [&]() {
		if (ImGui::IsItemActivated() && !generator_inspector_edit_before_.has_value()) {
			generator_inspector_edit_before_ = SerializeEntity(entity);
		}
	};

	auto finish_edit = [&]() {
		if (!ImGui::IsItemDeactivatedAfterEdit() || !generator_inspector_edit_before_.has_value()) {
			return;
		}

		const SerializedEntity before{ std::move(*generator_inspector_edit_before_) };
		generator_inspector_edit_before_.reset();
		const SerializedEntity after{ SerializeEntity(entity) };
		json before_json = before;
		json after_json	 = after;
		if (before_json == after_json) {
			return;
		}

		Scene* scene_ptr{ &scene };
		const UUID uuid{ entity.Get<UUID>() };
		auto apply = [scene_ptr, uuid](const SerializedEntity& snapshot) {
			if (Entity target{ scene_ptr->GetEntity(uuid) }) {
				DeserializeEntity(snapshot, target);
				scene_ptr->Refresh();
			}
		};

		ctx.undo.PushApplied(
			"Modify Generator", [apply, before]() mutable { apply(before); },
			[apply, after]() mutable { apply(after); }
		);
		ctx.local.state.is_dirty = true;
	};

	if (entity.Has<Tag>()) {
		std::string name{ entity.Get<Tag>().value };
		if (ImGui::InputText("Name##Generator", &name)) {
			entity.Get<Tag>().value = std::move(name);
		}
		begin_edit();
		finish_edit();
	}

	if (entity.Has<Transform>()) {
		Transform transform{ GetWorldTransform(entity) };
		float position[2]{ transform.position.x, transform.position.y };
		if (ImGui::DragFloat2("Position##Generator", position, 1.0f)) {
			transform.position = {
				position[0],
				position[1],
			};
			SetWorldTransform(entity, transform);
		}
		begin_edit();
		finish_edit();
	}

	ImGui::Checkbox("Enabled##Generator", &data.enabled);
	begin_edit();
	finish_edit();

	const char* geometry{ data.geometry == PaintGeneratorGeometry::Infinite	   ? "Infinite"
						  : data.geometry == PaintGeneratorGeometry::Line	   ? "Line"
						  : data.geometry == PaintGeneratorGeometry::Rectangle ? "Rectangle"
																			   : "Brush" };
	ImGui::Text("Geometry: %s", geometry);

	if (data.geometry == PaintGeneratorGeometry::BrushStroke) {
		ImGui::SeparatorText("Brush Stroke Diameters");

		struct StrokeRow {
			std::uint32_t id{};
			int diameter{ 1 };
		};

		std::vector<StrokeRow> strokes;

		for (const auto& point : data.stroke_points) {
			// Legacy single-stroke generators deserialize with stroke_id == 0.
			const std::uint32_t id{ point.stroke_id };
			const auto it{ std::ranges::find_if(strokes, [id](const StrokeRow& row) {
				return row.id == id;
			}) };
			if (it == strokes.end()) {
				strokes.push_back(
					StrokeRow{
						.id		  = id,
						.diameter = std::max(1, point.diameter),
					}
				);
			}
		}

		std::ranges::sort(strokes, [](const StrokeRow& a, const StrokeRow& b) {
			return a.id < b.id;
		});

		ImGui::TextDisabled(
			"Each authored stroke stays separate even when two strokes have the same diameter."
		);

		if (ImGui::BeginTable("##GeneratorBrushStrokes", 2, ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("Stroke", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("Diameter", ImGuiTableColumnFlags_WidthFixed, 110.0f);
			ImGui::TableHeadersRow();

			for (std::size_t index{}; index < strokes.size(); ++index) {
				const StrokeRow row{ strokes[index] };
				ImGui::PushID(static_cast<int>(row.id));
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::Text("Stroke %d", static_cast<int>(index + 1));

				ImGui::TableSetColumnIndex(1);
				int diameter{ row.diameter };
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::DragInt("##Diameter", &diameter, 0.15f, 1, 128)) {
					diameter = std::clamp(diameter, 1, 128);
					for (auto& point : data.stroke_points) {
						if (point.stroke_id == row.id) {
							point.diameter = diameter;
						}
					}
				}
				begin_edit();
				finish_edit();
				ImGui::PopID();
			}

			ImGui::EndTable();
		}

		ImGui::Text("Brush strokes: %d", static_cast<int>(strokes.size()));
	}

	if (data.geometry == PaintGeneratorGeometry::Line) {
		float start[2]{ data.start.x, data.start.y };
		float end[2]{ data.end.x, data.end.y };

		if (ImGui::DragFloat2("Start##Generator", start, 1.0f)) {
			data.start = {
				start[0],
				start[1],
			};
		}
		begin_edit();
		finish_edit();

		if (ImGui::DragFloat2("End##Generator", end, 1.0f)) {
			data.end = {
				end[0],
				end[1],
			};
		}
		begin_edit();
		finish_edit();
	}

	if (data.geometry == PaintGeneratorGeometry::Rectangle) {
		float start[2]{ data.start.x, data.start.y };
		float end[2]{ data.end.x, data.end.y };

		if (ImGui::DragFloat2("Start##Generator", start, 1.0f)) {
			data.start = {
				start[0],
				start[1],
			};
		}
		begin_edit();
		finish_edit();

		if (ImGui::DragFloat2("End##Generator", end, 1.0f)) {
			data.end = {
				end[0],
				end[1],
			};
		}
		begin_edit();
		finish_edit();
	}

	const bool active{ IsActiveBrushGenerator(entity) };
	if (active) {
		ImGui::SeparatorText("Live Generator");
		ImGui::TextDisabled("Consecutive Brush strokes are being added to this generator.");

		ImGui::BeginDisabled(data.stroke_points.empty());
		if (ImGui::Button("Undo Stroke")) {
			UndoActiveBrushStroke(ctx, scene);
			ImGui::EndDisabled();
			return;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		if (ImGui::Button("Finish Generator")) {
			FinishActiveBrushGenerator(ctx, scene);
			return;
		}

		ImGui::SameLine();
		if (ImGui::Button("Cancel Generator")) {
			CancelActiveBrushGenerator(ctx, scene);
			return;
		}
	}

	ImGui::Separator();

	ImGui::BeginDisabled(data.geometry == PaintGeneratorGeometry::Infinite);
	if (ImGui::Button("Bake Generator")) {
		BakeGenerator(ctx, scene, entity);
		ImGui::EndDisabled();
		return;
	}
	ImGui::EndDisabled();

	if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled) &&
		data.geometry == PaintGeneratorGeometry::Infinite) {
		ImGui::SetTooltip("Infinite generators cannot be baked without a finite region.");
	}
}

void PaintEditor::DrawTileInspector(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);
	if (!inspected_texture_.has_value()) {
		ImGui::TextDisabled("Select a tile in the Tiles tab.");
		return;
	}
	const TextureKey texture{ *inspected_texture_ };
	auto& settings{ GetSliceSettings(ctx, texture) };
	const TileLibraryEntry* selected{ FindTileEntry(selected_tile_entry_id_) };
	if (selected) {
		ImGui::Text("%s", selected->name.c_str());
	}
	ImGui::TextDisabled("%s", texture.value.c_str());
	ImGui::Separator();
	int tile_size[2]{ settings.tile_size.x, settings.tile_size.y };
	int margin[2]{ settings.margin.x, settings.margin.y };
	int spacing[2]{ settings.spacing.x, settings.spacing.y };
	bool rebuild{};
	if (ImGui::DragInt2("Tile Size", tile_size, 1.0f, 1, 8192)) {
		settings.tile_size = { std::max(1, tile_size[0]), std::max(1, tile_size[1]) };
		rebuild			   = true;
	}
	if (ImGui::DragInt2("Margin", margin, 1.0f, 0, 8192)) {
		settings.margin = { std::max(0, margin[0]), std::max(0, margin[1]) };
		rebuild			= true;
	}
	if (ImGui::DragInt2("Spacing", spacing, 1.0f, 0, 8192)) {
		settings.spacing = { std::max(0, spacing[0]), std::max(0, spacing[1]) };
		rebuild			 = true;
	}
	if (rebuild) {
		std::string group{ selected ? selected->group : "Ungrouped" };
		std::string base_name{ selected ? selected->name : TextureDisplayName(texture) };
		V2_int size{};
		for (const auto& record :
			 ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets()) {
			if (record.kind == AssetKind::Texture &&
				record.key == static_cast<const AssetKey&>(texture) &&
				record.metadata.dimensions.has_value()) {
				size = *record.metadata.dimensions;
				break;
			}
		}
		std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
			return entry.texture == texture;
		});
		if (size.IsPositive()) {
			const int columns{ std::max(
				0, (size.x - settings.margin.x + settings.spacing.x) /
					   std::max(1, settings.tile_size.x + settings.spacing.x)
			) };
			const int rows{ std::max(
				0, (size.y - settings.margin.y + settings.spacing.y) /
					   std::max(1, settings.tile_size.y + settings.spacing.y)
			) };
			int index{};
			for (int y{}; y < rows; ++y) {
				for (int x{}; x < columns; ++x) {
					V2_int slice{ x, y };
					tile_library_.push_back(
						TileLibraryEntry{ .id	   = TileEntryId(texture, slice),
										  .name	   = base_name + "_" + std::to_string(index++),
										  .texture = texture,
										  .slice   = slice,
										  .group   = group }
					);
				}
			}
			if (!tile_library_.empty()) {
				auto& entry{ tile_library_.back() };
				selected_tile_entry_id_ = entry.id;
				inspected_slice_		= entry.slice;
			}
		}
		SaveProjectLibrary(ctx);
	}
	ImGui::Text("Selected slice: %d, %d", inspected_slice_.x, inspected_slice_.y);
}

V2_float PaintEditor::ActiveGridSize(const Scene& scene) const {
	if (const SceneLayer* layer{ ResolveActiveLayer(scene) };
		layer && layer->kind == SceneLayerKind::Tile) {
		if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			return map.GetData().cell_size;
		}
	}
	return entity_grid_size_;
}

V2_float PaintEditor::ActiveGridOrigin(const Scene& scene) const {
	if (const SceneLayer* layer{ ResolveActiveLayer(scene) };
		layer && layer->kind == SceneLayerKind::Tile) {
		if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			return GetWorldPosition(map);
		}
	}
	return entity_grid_offset_;
}

V2_int PaintEditor::WorldToActiveCell(const Scene& scene, V2_float world) const {
	const V2_float size{ ActiveGridSize(scene) };
	const V2_float origin{ ActiveGridOrigin(scene) };
	return {
		static_cast<int>(std::floor((world.x - origin.x) / std::max(1.0f, size.x))),
		static_cast<int>(std::floor((world.y - origin.y) / std::max(1.0f, size.y))),
	};
}

V2_float PaintEditor::ActiveCellToWorld(const Scene& scene, V2_int cell) const {
	return ActiveGridOrigin(scene) +
		   V2_float{ static_cast<float>(cell.x), static_cast<float>(cell.y) } *
			   ActiveGridSize(scene);
}

std::vector<V2_int> PaintEditor::BrushCells(V2_int center) const {
	std::vector<V2_int> cells;
	const int diameter{ std::max(1, brush_diameter_) };
	const int low{ -(diameter / 2) };
	const int high{ low + diameter - 1 };
	const float radius{ static_cast<float>(diameter) * 0.5f };

	for (int y{ low }; y <= high; ++y) {
		for (int x{ low }; x <= high; ++x) {
			if (brush_shape_ == PaintBrushShape::Circle) {
				const float dx{ static_cast<float>(x) + 0.5f };
				const float dy{ static_cast<float>(y) + 0.5f };
				if (std::sqrt(dx * dx + dy * dy) > radius) {
					continue;
				}
			}
			cells.emplace_back(center + V2_int{ x, y });
		}
	}
	return cells;
}

std::vector<V2_int> PaintEditor::SelectionBrushCells(V2_int center) const {
	std::vector<V2_int> cells;
	const int diameter{ std::max(1, selection_diameter_) };
	const int low{ -(diameter / 2) };
	const int high{ low + diameter - 1 };
	const float radius{ static_cast<float>(diameter) * 0.5f };
	for (int y{ low }; y <= high; ++y) {
		for (int x{ low }; x <= high; ++x) {
			if (selection_brush_shape_ == PaintBrushShape::Circle) {
				const float dx{ static_cast<float>(x) + 0.5f };
				const float dy{ static_cast<float>(y) + 0.5f };
				if (std::sqrt(dx * dx + dy * dy) > radius) {
					continue;
				}
			}
			cells.emplace_back(center + V2_int{ x, y });
		}
	}
	return cells;
}

std::vector<V2_int> PaintEditor::LineCells(V2_int a, V2_int b) const {
	std::vector<V2_int> cells;
	const int dx{ std::abs(b.x - a.x) };
	const int dy{ -std::abs(b.y - a.y) };
	const int sx{ a.x < b.x ? 1 : -1 };
	const int sy{ a.y < b.y ? 1 : -1 };
	int error{ dx + dy };
	int step{};

	for (;;) {
		if (step % std::max(1, line_spacing_) == 0) {
			const int half{ std::max(1, line_thickness_) / 2 };
			for (int oy{ -half }; oy <= half; ++oy) {
				for (int ox{ -half }; ox <= half; ++ox) {
					cells.emplace_back(a + V2_int{ ox, oy });
				}
			}
		}
		if (a == b) {
			break;
		}
		const int e2{ 2 * error };
		if (e2 >= dy) {
			error += dy;
			a.x	  += sx;
		}
		if (e2 <= dx) {
			error += dx;
			a.y	  += sy;
		}
		++step;
	}
	std::ranges::sort(cells, {}, [](V2_int value) { return std::pair{ value.y, value.x }; });
	cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
	return cells;
}

std::vector<V2_int> PaintEditor::RectangleCells(V2_int a, V2_int b) const {
	const int min_x{ std::min(a.x, b.x) };
	const int max_x{ std::max(a.x, b.x) };
	const int min_y{ std::min(a.y, b.y) };
	const int max_y{ std::max(a.y, b.y) };
	std::vector<V2_int> cells;
	for (int y{ min_y }; y <= max_y; ++y) {
		for (int x{ min_x }; x <= max_x; ++x) {
			const int left{ x - min_x };
			const int right{ max_x - x };
			const int top{ y - min_y };
			const int bottom{ max_y - y };
			bool include{};
			switch (area_mode_) {
				case PaintAreaMode::Fill: include = true; break;
				case PaintAreaMode::RandomFill:
					include = Hash01({ x, y }) <= recipe_.density;
					break;
				case PaintAreaMode::Outline:
					include = left < area_thickness_ || right < area_thickness_ ||
							  top < area_thickness_ || bottom < area_thickness_;
					break;
				case PaintAreaMode::Corners: {
					const int thickness{ std::max(1, area_thickness_) };
					include = (left < thickness && top < thickness) ||
							  (right < thickness && top < thickness) ||
							  (left < thickness && bottom < thickness) ||
							  (right < thickness && bottom < thickness);
					break;
				}
			}
			if (include) {
				cells.emplace_back(x, y);
			}
		}
	}
	return cells;
}

bool PaintEditor::CoveragePass(V2_int cell, V2_int first, V2_int last) const {
	if (recipe_.coverage == PaintCoverageMode::Solid) {
		return true;
	}
	const float selector{ Hash01(cell) };
	if (recipe_.coverage == PaintCoverageMode::RandomDensity) {
		return selector <= recipe_.density;
	}
	const V2_float center{
		(static_cast<float>(first.x + last.x) + 1.0f) * 0.5f,
		(static_cast<float>(first.y + last.y) + 1.0f) * 0.5f,
	};
	const V2_float p{ static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y) + 0.5f };
	const V2_float span{
		std::max(1.0f, static_cast<float>(last.x - first.x + 1)) * 0.5f,
		std::max(1.0f, static_cast<float>(last.y - first.y + 1)) * 0.5f,
	};
	const float nx{ (p.x - center.x) / span.x };
	const float ny{ (p.y - center.y) / span.y };
	const float normalized{ std::min(1.0f, std::sqrt(nx * nx + ny * ny)) };
	const float inner{ std::clamp(recipe_.radial_inner, 0.0f, 1.0f) };
	const float outer{ std::max(inner + 0.001f, std::clamp(recipe_.radial_outer, 0.0f, 1.0f)) };
	const float t{ std::clamp((normalized - inner) / (outer - inner), 0.0f, 1.0f) };
	return selector <= (1.0f - t) * recipe_.density;
}

namespace {

[[nodiscard]] std::uint32_t NoiseHash2(int x, int y, std::uint32_t seed) {
	std::uint32_t h{ seed ^ 0x9E3779B9u };
	h ^= static_cast<std::uint32_t>(x) * 0x85EBCA6Bu;
	h  = (h << 13u) | (h >> 19u);
	h ^= static_cast<std::uint32_t>(y) * 0xC2B2AE35u;
	h *= 0x27D4EB2Du;
	h ^= h >> 15u;
	return h;
}

[[nodiscard]] float NoiseFade(float t) {
	return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

[[nodiscard]] float NoiseGrad(std::uint32_t h, float x, float y) {
	switch (h & 7u) {
		case 0:	 return x + y;
		case 1:	 return -x + y;
		case 2:	 return x - y;
		case 3:	 return -x - y;
		case 4:	 return x;
		case 5:	 return -x;
		case 6:	 return y;
		default: return -y;
	}
}

[[nodiscard]] float Perlin01(float x, float y, std::uint32_t seed) {
	const int x0{ static_cast<int>(std::floor(x)) };
	const int y0{ static_cast<int>(std::floor(y)) };
	const float tx{ x - static_cast<float>(x0) };
	const float ty{ y - static_cast<float>(y0) };
	const float u{ NoiseFade(tx) };
	const float v{ NoiseFade(ty) };
	const float a{ NoiseGrad(NoiseHash2(x0, y0, seed), tx, ty) };
	const float b{ NoiseGrad(NoiseHash2(x0 + 1, y0, seed), tx - 1.0f, ty) };
	const float c{ NoiseGrad(NoiseHash2(x0, y0 + 1, seed), tx, ty - 1.0f) };
	const float d{ NoiseGrad(NoiseHash2(x0 + 1, y0 + 1, seed), tx - 1.0f, ty - 1.0f) };
	const float ab{ a + (b - a) * u };
	const float cd{ c + (d - c) * u };
	return std::clamp((ab + (cd - ab) * v) * 0.5f + 0.5f, 0.0f, 1.0f);
}

[[nodiscard]] float Value01(float x, float y, std::uint32_t seed) {
	const int x0{ static_cast<int>(std::floor(x)) };
	const int y0{ static_cast<int>(std::floor(y)) };
	const float tx{ x - static_cast<float>(x0) };
	const float ty{ y - static_cast<float>(y0) };
	const float u{ NoiseFade(tx) };
	const float v{ NoiseFade(ty) };
	auto value = [&](int xx, int yy) {
		return static_cast<float>(NoiseHash2(xx, yy, seed) & 0x00ffffffu) /
			   static_cast<float>(0x00ffffffu);
	};
	const float ab{ value(x0, y0) + (value(x0 + 1, y0) - value(x0, y0)) * u };
	const float cd{ value(x0, y0 + 1) + (value(x0 + 1, y0 + 1) - value(x0, y0 + 1)) * u };
	return std::clamp(ab + (cd - ab) * v, 0.0f, 1.0f);
}

[[nodiscard]] float Simplex01(float xin, float yin, std::uint32_t seed) {
	constexpr float f2{ 0.3660254037844386f };
	constexpr float g2{ 0.2113248654051871f };
	const float s{ (xin + yin) * f2 };
	const int i{ static_cast<int>(std::floor(xin + s)) };
	const int j{ static_cast<int>(std::floor(yin + s)) };
	const float t{ static_cast<float>(i + j) * g2 };
	const float x0{ xin - (static_cast<float>(i) - t) };
	const float y0{ yin - (static_cast<float>(j) - t) };
	const int i1{ x0 > y0 ? 1 : 0 };
	const int j1{ x0 > y0 ? 0 : 1 };
	const float x1{ x0 - static_cast<float>(i1) + g2 };
	const float y1{ y0 - static_cast<float>(j1) + g2 };
	const float x2{ x0 - 1.0f + 2.0f * g2 };
	const float y2{ y0 - 1.0f + 2.0f * g2 };
	auto contribution = [&](int gx, int gy, float x, float y) {
		float q{ 0.5f - x * x - y * y };
		if (q <= 0.0f) {
			return 0.0f;
		}
		q *= q;
		return q * q * NoiseGrad(NoiseHash2(gx, gy, seed), x, y);
	};
	return std::clamp(
		0.5f + 35.0f * (contribution(i, j, x0, y0) + contribution(i + i1, j + j1, x1, y1) +
						contribution(i + 1, j + 1, x2, y2)),
		0.0f, 1.0f
	);
}

[[nodiscard]] float BaseNoise01(NoiseType type, float x, float y, std::uint32_t seed) {
	switch (type) {
		case NoiseType::Perlin:	 return Perlin01(x, y, seed);
		case NoiseType::Value:	 return Value01(x, y, seed);
		case NoiseType::Simplex: return Simplex01(x, y, seed);
	}
	return Perlin01(x, y, seed);
}

[[nodiscard]] float FractalNoise01(
	V2_float world, NoiseType type, int seed, float frequency, int octaves, float lacunarity,
	float persistence, V2_float offset
) {
	float f{ std::max(0.00001f, frequency) };
	float amplitude{ 1.0f };
	float total{};
	float weight{};
	for (int octave{}; octave < std::clamp(octaves, 1, 12); ++octave) {
		total	  += BaseNoise01(
						 type, (world.x + offset.x) * f, (world.y + offset.y) * f,
						 static_cast<std::uint32_t>(seed + octave * 1013)
					 ) *
					 amplitude;
		weight	  += amplitude;
		f		  *= std::max(1.0f, lacunarity);
		amplitude *= std::clamp(persistence, 0.0f, 1.0f);
	}
	return weight > 0.0f ? std::clamp(total / weight, 0.0f, 1.0f) : 0.0f;
}

} // namespace

float PaintEditor::NoiseValue(V2_float world, const PaintNoiseState& noise) const {
	return FractalNoise01(
		world, noise.type, noise.seed, noise.frequency, noise.octaves, noise.lacunarity,
		noise.persistence, noise.offset
	);
}

const PaintNoiseThreshold* PaintEditor::ResolveNoiseThreshold(V2_float world) const {
	if (recipe_.source_kind != PaintSourceKind::Noise) {
		return nullptr;
	}
	const float value{ NoiseValue(world, recipe_.noise) };
	for (const auto& region : recipe_.noise.thresholds) {
		if (!region.enabled) {
			continue;
		}
		const float lo{ std::min(region.minimum, region.maximum) };
		const float hi{ std::max(region.minimum, region.maximum) };
		if (value >= lo && (value < hi || (hi >= 0.99999f && value <= 1.0f))) {
			return &region;
		}
	}
	return nullptr;
}

PaintGeneratorRecipe PaintEditor::CaptureGeneratorRecipe(const SceneLayer& layer) const {
	PaintGeneratorRecipe result;
	result.source_kind = static_cast<PaintGeneratorSourceKind>(recipe_.source_kind);
	result.coverage	   = static_cast<PaintGeneratorCoverageMode>(recipe_.coverage);
	if (layer.kind == SceneLayerKind::Tile && tile_source_) {
		result.tile = PaintGeneratorTileSource{
			.texture			 = tile_source_.texture,
			.texture_coordinates = tile_source_.texture_coordinates,
			.pixel_size			 = tile_source_.pixel_size,
			.origin				 = recipe_.tile_origin,
		};
	} else if (layer.kind == SceneLayerKind::Entity && prefab_source_) {
		result.prefab = prefab_source_;
	}

	if (const auto* set{ FindWeightedTileSet(recipe_.weighted_tile_set_name) }) {
		for (const auto& entry : set->entries) {
			if (entry.source) {
				result.weighted_tiles.push_back(
					{ PaintGeneratorTileSource{ .texture = entry.source.texture,
												.texture_coordinates =
													entry.source.texture_coordinates,
												.pixel_size = entry.source.pixel_size,
												.origin		= recipe_.tile_origin },
					  entry.weight }
				);
			}
		}
	}
	if (const auto* set{ FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) }) {
		for (const auto& entry : set->entries) {
			if (entry.prefab) {
				result.weighted_prefabs.push_back({ entry.prefab, entry.weight });
			}
		}
	}
	if (recipe_.checker_tile) {
		result.checker_tile =
			PaintGeneratorTileSource{ .texture = recipe_.checker_tile->texture,
									  .texture_coordinates =
										  recipe_.checker_tile->texture_coordinates,
									  .pixel_size = recipe_.checker_tile->pixel_size,
									  .origin	  = recipe_.tile_origin };
	}
	result.checker_prefab = recipe_.checker_prefab;
	if (const auto* rules{ FindAutotileRuleSet(recipe_.autotile_ruleset_name) }) {
		result.autotile_format = static_cast<PaintGeneratorAutotileFormat>(rules->format);
		result.autotile_tiles.resize(rules->tiles.size());
		for (std::size_t i{}; i < rules->tiles.size(); ++i) {
			if (rules->tiles[i]) {
				result.autotile_tiles[i] = PaintGeneratorTileSource{
					.texture			 = rules->tiles[i]->texture,
					.texture_coordinates = rules->tiles[i]->texture_coordinates,
					.pixel_size			 = rules->tiles[i]->pixel_size,
					.origin				 = Origin::TopLeft,
				};
			}
		}
	}
	result.tile_placement = static_cast<PaintGeneratorTilePlacementMode>(recipe_.tile_placement);
	result.tile_origin	  = recipe_.tile_origin;
	result.entity_origin  = recipe_.entity_origin;
	result.density		  = recipe_.density;
	result.radial_inner	  = recipe_.radial_inner;
	result.radial_outer	  = recipe_.radial_outer;
	result.min_spacing	  = recipe_.min_spacing;
	result.avoid_exclusion_mask	  = recipe_.avoid_exclusion_mask;
	result.link_prefab_instances  = recipe_.link_prefab_instances;
	result.random_rotation		  = recipe_.random_rotation;
	result.rotation_min			  = recipe_.rotation_min;
	result.rotation_max			  = recipe_.rotation_max;
	result.random_scale			  = recipe_.random_scale;
	result.scale_min			  = recipe_.scale_min;
	result.scale_max			  = recipe_.scale_max;
	result.show_noise_preview	  = recipe_.show_noise_preview;
	result.show_generated_preview = recipe_.show_generated_preview;
	result.noise_preview_alpha	  = recipe_.noise_preview_alpha;
	result.noise.type			  = recipe_.noise.type;
	result.noise.seed			  = recipe_.noise.seed;
	result.noise.frequency		  = recipe_.noise.frequency;
	result.noise.octaves		  = recipe_.noise.octaves;
	result.noise.lacunarity		  = recipe_.noise.lacunarity;
	result.noise.persistence	  = recipe_.noise.persistence;
	result.noise.offset			  = recipe_.noise.offset;
	for (const auto& region : recipe_.noise.thresholds) {
		PaintGeneratorNoiseThreshold captured{
			.minimum	 = region.minimum,
			.maximum	 = region.maximum,
			.enabled	 = region.enabled,
			.source_kind = static_cast<PaintGeneratorSourceKind>(region.source_kind),
			.origin		 = region.origin,
		};
		if (region.tile) {
			captured.tile = PaintGeneratorTileSource{
				.texture			 = region.tile->texture,
				.texture_coordinates = region.tile->texture_coordinates,
				.pixel_size			 = region.tile->pixel_size,
				.origin				 = region.origin,
			};
		}
		if (region.prefab) {
			captured.prefab = region.prefab;
		}
		if (const auto* set{ FindWeightedTileSet(region.weighted_tile_set_name) }) {
			for (const auto& entry : set->entries) {
				if (entry.source) {
					captured.weighted_tiles.push_back(
						{ PaintGeneratorTileSource{ .texture = entry.source.texture,
													.texture_coordinates =
														entry.source.texture_coordinates,
													.pixel_size = entry.source.pixel_size,
													.origin		= region.origin },
						  entry.weight }
					);
				}
			}
		}
		if (const auto* set{ FindWeightedPrefabSet(region.weighted_prefab_set_name) }) {
			for (const auto& entry : set->entries) {
				if (entry.prefab) {
					captured.weighted_prefabs.push_back({ entry.prefab, entry.weight });
				}
			}
		}
		result.noise.thresholds.emplace_back(std::move(captured));
	}
	return result;
}

void PaintEditor::DrawTilemaps(
	EditorContext& ctx, Scene& scene, ImDrawList* draw, Viewport presentation_viewport,
	const FrameContext& frame
) const {
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	for (const SceneLayer& layer : scene.GetLayers().GetLayers()) {
		if (layer.kind != SceneLayerKind::Tile || !layer.visible) {
			continue;
		}

		for (Entity entity : scene.GetLayers().GetRootEntities(scene, layer.id)) {
			if (!IsTilemap(entity)) {
				continue;
			}

			Tilemap tilemap{ entity };
			const auto& data{ tilemap.GetData() };

			for (const TilemapTile& tile : data.tiles) {
				if (!tile.texture) {
					continue;
				}

				const auto record_it{ std::ranges::find_if(records, [&](const auto& record) {
					return record.kind == AssetKind::Texture &&
						   record.key == static_cast<const AssetKey&>(tile.texture);
				}) };

				const V2_float visual_size{
					tile.pixel_size.IsPositive()
						? V2_float{
							static_cast<float>(tile.pixel_size.x),
							static_cast<float>(tile.pixel_size.y),
						}
						: data.cell_size
				};

				const V2_float anchor{ tilemap.CellToWorld(tile.coordinate) + tile.offset };
				const V2_float center{ anchor + GetOffset(tile.origin, visual_size) };
				const V2_float mn{ center - visual_size * 0.5f };
				const V2_float mx{ center + visual_size * 0.5f };

				const ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
				const ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };

				if (record_it != records.end() && record_it->preview.has_value()) {
					const auto& uv{ tile.texture_coordinates };
					draw->AddImageQuad(
						static_cast<ImTextureID>(record_it->preview->texture), p0, { p1.x, p0.y },
						p1, { p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]),
						ToImGui(uv[3]), tile.tint.ToUint32(ColorPacking::ABGR)
					);
				} else {
					draw->AddRectFilled(p0, p1, IM_COL32(120, 130, 150, 210));
				}
			}

			// Exclusion cells are authored paint data, so keep them visible independently of
			// streaming/debug settings. They use the same translucent-red convention as the
			// sandbox paint demo.
			for (const V2_int cell : data.exclusion_mask) {
				const V2_float mn{ tilemap.CellToWorld(cell) };
				const V2_float mx{ mn + data.cell_size };
				const ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
				const ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };

				draw->AddRectFilled(p0, p1, IM_COL32(255, 26, 26, 46));
				draw->AddRect(p0, p1, IM_COL32(255, 72, 72, 120), 0.0f, 0, 1.0f);
			}
		}
	}
}

void PaintEditor::DrawGenerators(
	EditorContext& ctx, Scene& scene, ImDrawList* draw, Viewport image_viewport,
	Viewport presentation_viewport, const FrameContext& frame
) const {
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	Entity selected_generator{
		selected_generator_.has_value() ? scene.GetEntity(*selected_generator_) : Entity{}
	};
	if (!selected_generator) {
		Entity hierarchy_selected{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
		if (hierarchy_selected && IsPaintGenerator(hierarchy_selected)) {
			selected_generator = hierarchy_selected;
		}
	}
	const V2_float view_a{ ScreenToWorld(image_viewport.position, frame, presentation_viewport) };
	const V2_float view_b{
		ScreenToWorld(image_viewport.position + image_viewport.size, frame, presentation_viewport)
	};
	const float view_min_x{ std::min(view_a.x, view_b.x) };
	const float view_max_x{ std::max(view_a.x, view_b.x) };
	const float view_min_y{ std::min(view_a.y, view_b.y) };
	const float view_max_y{ std::max(view_a.y, view_b.y) };

	auto line_cells = [](V2_int a, V2_int b, int thickness, int spacing) {
		std::vector<V2_int> result;
		const int dx{ std::abs(b.x - a.x) };
		const int dy{ -std::abs(b.y - a.y) };
		const int sx{ a.x < b.x ? 1 : -1 };
		const int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};
		for (;;) {
			if (step % std::max(1, spacing) == 0) {
				const int half{ std::max(1, thickness) / 2 };
				for (int y{ -half }; y <= half; ++y) {
					for (int x{ -half }; x <= half; ++x) {
						result.emplace_back(a + V2_int{ x, y });
					}
				}
			}
			if (a == b) {
				break;
			}
			const int e2{ 2 * error };
			if (e2 >= dy) {
				error += dy;
				a.x	  += sx;
			}
			if (e2 <= dx) {
				error += dx;
				a.y	  += sy;
			}
			++step;
		}
		return result;
	};

	for (const SceneLayer& layer : scene.GetLayers().GetLayers()) {
		if (!layer.visible) {
			continue;
		}
		for (Entity entity : scene.GetLayers().GetRootEntities(scene, layer.id)) {
			if (!IsPaintGenerator(entity)) {
				continue;
			}
			PaintGenerator generator{ entity };
			const auto& data{ generator.GetData() };
			if (!data.enabled || !data.grid_size.IsPositive()) {
				continue;
			}
			const V2_float entity_offset{ GetWorldPosition(generator) };
			const V2_float origin{ data.grid_offset + entity_offset };
			auto to_cell = [&](V2_float world) {
				return V2_int{
					static_cast<int>(std::floor((world.x - origin.x) / data.grid_size.x)),
					static_cast<int>(std::floor((world.y - origin.y) / data.grid_size.y))
				};
			};
			auto cell_world = [&](V2_int cell) {
				return origin + V2_float{ static_cast<float>(cell.x), static_cast<float>(cell.y) } *
									data.grid_size;
			};
			const V2_int visible_min{ to_cell({ view_min_x, view_min_y }) - V2_int{ 1, 1 } };
			const V2_int visible_max{ to_cell({ view_max_x, view_max_y }) + V2_int{ 1, 1 } };
			if (visible_max.x - visible_min.x > 1024 || visible_max.y - visible_min.y > 1024) {
				continue;
			}

			std::unordered_set<V2_int> cells;
			auto add_if_visible = [&](V2_int cell) {
				if (cell.x >= visible_min.x && cell.x <= visible_max.x && cell.y >= visible_min.y &&
					cell.y <= visible_max.y) {
					cells.insert(cell);
				}
			};
			if (data.geometry == PaintGeneratorGeometry::Infinite) {
				for (int y{ visible_min.y }; y <= visible_max.y; ++y) {
					for (int x{ visible_min.x }; x <= visible_max.x; ++x) {
						cells.insert({ x, y });
					}
				}
			} else if (data.geometry == PaintGeneratorGeometry::Line) {
				for (V2_int cell : line_cells(
						 to_cell(data.start + entity_offset), to_cell(data.end + entity_offset),
						 data.line_thickness, data.line_spacing
					 )) {
					add_if_visible(cell);
				}
			} else if (data.geometry == PaintGeneratorGeometry::Rectangle) {
				const V2_int a{ to_cell(data.start + entity_offset) };
				const V2_int b{ to_cell(data.end + entity_offset) };
				const int min_x{ std::min(a.x, b.x) }, max_x{ std::max(a.x, b.x) },
					min_y{ std::min(a.y, b.y) }, max_y{ std::max(a.y, b.y) };
				for (int y{ min_y }; y <= max_y; ++y) {
					for (int x{ min_x }; x <= max_x; ++x) {
						const int l{ x - min_x }, r{ max_x - x }, t{ y - min_y },
							bottom{ max_y - y };
						bool include{};
						switch (data.area_mode) {
							case PaintGeneratorAreaMode::Fill: include = true; break;
							case PaintGeneratorAreaMode::RandomFill:
								include = Hash01({ x, y }) <= data.random_fill_density;
								break;
							case PaintGeneratorAreaMode::Outline:
								include = l < data.area_thickness || r < data.area_thickness ||
										  t < data.area_thickness || bottom < data.area_thickness;
								break;
							case PaintGeneratorAreaMode::Corners: {
								const int thickness{ std::max(1, data.area_thickness) };
								include = (l < thickness && t < thickness) ||
										  (r < thickness && t < thickness) ||
										  (l < thickness && bottom < thickness) ||
										  (r < thickness && bottom < thickness);
								break;
							}
						}
						if (include) {
							add_if_visible({ x, y });
						}
					}
				}
			} else {
				for (const auto& point : data.stroke_points) {
					const V2_int center{ to_cell(point.position + entity_offset) };
					const int diameter{ std::max(1, point.diameter) };
					const int low{ -(diameter / 2) }, high{ low + diameter - 1 };
					const float radius{ static_cast<float>(diameter) * 0.5f };
					for (int y{ low }; y <= high; ++y) {
						for (int x{ low }; x <= high; ++x) {
							if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
								const float dx{ static_cast<float>(x) + 0.5f },
									dy{ static_cast<float>(y) + 0.5f };
								if (std::sqrt(dx * dx + dy * dy) > radius) {
									continue;
								}
							}
							add_if_visible(center + V2_int{ x, y });
						}
					}
				}
			}

			for (V2_int cell : cells) {
				if (generator.IsSuppressed(cell)) {
					continue;
				}
				if (data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity &&
					Hash01(cell) > data.recipe.density) {
					continue;
				}
				const V2_float mn{ cell_world(cell) };
				const V2_float mx{ mn + data.grid_size };
				const V2_float center{ mn + data.grid_size * 0.5f };
				const float noise_value{
					data.recipe.source_kind == PaintGeneratorSourceKind::Noise
						? FractalNoise01(
							  center, data.recipe.noise.type, data.recipe.noise.seed,
							  data.recipe.noise.frequency, data.recipe.noise.octaves,
							  data.recipe.noise.lacunarity, data.recipe.noise.persistence,
							  data.recipe.noise.offset
						  )
						: 0.0f
				};
				if (data.recipe.show_noise_preview &&
					data.recipe.source_kind == PaintGeneratorSourceKind::Noise) {
					const int shade{ static_cast<int>(std::round(noise_value * 255.0f)) };
					const int alpha{ static_cast<int>(
						std::round(std::clamp(data.recipe.noise_preview_alpha, 0.0f, 1.0f) * 255.0f)
					) };
					draw->AddRectFilled(
						ToImGui(WorldToScreen(mn, frame, presentation_viewport)),
						ToImGui(WorldToScreen(mx, frame, presentation_viewport)),
						IM_COL32(shade, shade, shade, alpha)
					);
				}
				if (!data.recipe.show_generated_preview) {
					continue;
				}

				std::optional<PaintGeneratorTileSource> tile;
				std::optional<PrefabKey> prefab;
				switch (data.recipe.source_kind) {
					case PaintGeneratorSourceKind::Single:
						tile   = data.recipe.tile;
						prefab = data.recipe.prefab;
						break;
					case PaintGeneratorSourceKind::WeightedSet: {
						if (layer.kind == SceneLayerKind::Tile) {
							const auto i{ ChooseWeightedIndex(
								data.recipe.weighted_tiles, cell,
								[](const auto& e) { return e.weight; }
							) };
							if (i) {
								tile = data.recipe.weighted_tiles[*i].source;
							}
						} else {
							const auto i{ ChooseWeightedIndex(
								data.recipe.weighted_prefabs, cell,
								[](const auto& e) { return e.weight; }
							) };
							if (i) {
								prefab = data.recipe.weighted_prefabs[*i].prefab;
							}
						}
						break;
					}
					case PaintGeneratorSourceKind::Checkerboard:
						if ((cell.x + cell.y) & 1) {
							tile   = data.recipe.checker_tile;
							prefab = data.recipe.checker_prefab;
						} else {
							tile   = data.recipe.tile;
							prefab = data.recipe.prefab;
						}
						break;
					case PaintGeneratorSourceKind::Autotile: {
						const int index{ AutotileIndex(
							static_cast<PaintAutotileFormat>(data.recipe.autotile_format),
							[&](V2_int c) { return cells.contains(c); }, cell
						) };
						if (index >= 0 &&
							index < static_cast<int>(data.recipe.autotile_tiles.size())) {
							tile = data.recipe.autotile_tiles[static_cast<std::size_t>(index)];
						}
						break;
					}
					case PaintGeneratorSourceKind::Noise:
						for (const auto& region : data.recipe.noise.thresholds) {
							if (!region.enabled) {
								continue;
							}
							const float lo{ std::min(region.minimum, region.maximum) },
								hi{ std::max(region.minimum, region.maximum) };
							if (noise_value < lo ||
								(noise_value >= hi && !(hi >= 0.99999f && noise_value <= 1.0f))) {
								continue;
							}
							if (region.source_kind == PaintGeneratorSourceKind::WeightedSet) {
								if (layer.kind == SceneLayerKind::Tile) {
									const auto i{ ChooseWeightedIndex(
										region.weighted_tiles, cell,
										[](const auto& e) { return e.weight; }
									) };
									if (i) {
										tile = region.weighted_tiles[*i].source;
									}
								} else {
									const auto i{ ChooseWeightedIndex(
										region.weighted_prefabs, cell,
										[](const auto& e) { return e.weight; }
									) };
									if (i) {
										prefab = region.weighted_prefabs[*i].prefab;
									}
								}
							} else {
								tile   = region.tile;
								prefab = region.prefab;
							}
							break;
						}
						break;
				}
				const bool selected{ selected_generator && selected_generator == entity };
				if (layer.kind == SceneLayerKind::Tile && tile.has_value()) {
					const V2_float visual_size{ tile->pixel_size.IsPositive()
													? V2_float{ tile->pixel_size }
													: data.grid_size };
					if (data.recipe.tile_placement == PaintGeneratorTilePlacementMode::Tile) {
						const V2_int footprint{
							std::max(
								1, static_cast<int>(
									   std::ceil(visual_size.x / std::max(1.0f, data.grid_size.x))
								   )
							),
							std::max(
								1, static_cast<int>(
									   std::ceil(visual_size.y / std::max(1.0f, data.grid_size.y))
								   )
							),
						};
						const V2_int lattice_origin{ to_cell(data.start + entity_offset) };
						if (FloorMod(cell.x - lattice_origin.x, footprint.x) != 0 ||
							FloorMod(cell.y - lattice_origin.y, footprint.y) != 0) {
							continue;
						}
					}
					const auto record_it{ std::ranges::find_if(records, [&](const auto& r) {
						return r.kind == AssetKind::Texture &&
							   r.key == static_cast<const AssetKey&>(tile->texture);
					}) };
					const V2_float tile_center{ mn + GetOffset(tile->origin, visual_size) };
					const V2_float tile_min{ tile_center - visual_size * 0.5f };
					const V2_float tile_max{ tile_center + visual_size * 0.5f };
					if (record_it != records.end() && record_it->preview.has_value()) {
						const auto& uv{ tile->texture_coordinates };
						const ImVec2 p0{
							ToImGui(WorldToScreen(tile_min, frame, presentation_viewport))
						};
						const ImVec2 p1{
							ToImGui(WorldToScreen(tile_max, frame, presentation_viewport))
						};
						draw->AddImageQuad(
							static_cast<ImTextureID>(record_it->preview->texture), p0,
							{ p1.x, p0.y }, p1, { p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]),
							ToImGui(uv[2]), ToImGui(uv[3]), IM_COL32(255, 255, 255, 205)
						);
					}
				} else if (layer.kind == SceneLayerKind::Entity && prefab.has_value()) {
					const ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) },
						p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };
					draw->AddRectFilled(p0, p1, IM_COL32(255, 208, 70, 28));
					draw->AddRect(p0, p1, kPreview, 0.0f, 0, 1.2f);
					const ImVec2 c{ ToImGui(WorldToScreen(center, frame, presentation_viewport)) };
					draw->AddCircleFilled(c, 3.0f, kPreview, 8);
				}
				draw->AddRect(
					ToImGui(WorldToScreen(mn, frame, presentation_viewport)),
					ToImGui(WorldToScreen(mx, frame, presentation_viewport)),
					selected ? kPreview : kSelection, 0.0f, 0, selected ? 2.0f : 1.35f
				);
			}
		}
	}
}

void PaintEditor::DrawActiveGeneratorPreview(
	EditorContext& ctx, Scene& scene, ImDrawList* draw, Viewport presentation_viewport,
	const FrameContext& frame
) {
	if (!stroke_.active) {
		return;
	}
	if (tool_ != PaintTool::Brush && tool_ != PaintTool::Line && tool_ != PaintTool::Rectangle) {
		return;
	}
	if (recipe_.commit_mode != PaintCommitMode::KeepGenerator ||
		recipe_.operation == PaintBrushOperation::ExclusionMask) {
		return;
	}

	const SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked || !layer->visible) {
		return;
	}

	std::unordered_set<V2_int> cells;
	const V2_int first{ WorldToActiveCell(scene, stroke_.start_world) };
	const V2_int last{ WorldToActiveCell(scene, stroke_.current_world) };
	if (tool_ == PaintTool::Brush) {
		auto add_brush_cell = [&](V2_int center) {
			for (V2_int cell : BrushCells(center)) {
				cells.insert(cell);
			}
		};
		for (V2_float point : stroke_.generator_points) {
			add_brush_cell(WorldToActiveCell(scene, point));
		}

		const V2_int current_cell{ WorldToActiveCell(scene, stroke_.current_world) };
		if (!stroke_.generator_points.empty()) {
			const V2_int previous_cell{ WorldToActiveCell(scene, stroke_.generator_points.back()) };
			for (V2_int center : LineCells(previous_cell, current_cell)) {
				add_brush_cell(center);
			}
		} else {
			add_brush_cell(current_cell);
		}
	} else if (tool_ == PaintTool::Line) {
		for (V2_int cell : LineCells(first, last)) {
			cells.insert(cell);
		}
	} else {
		for (V2_int cell : RectangleCells(first, last)) {
			cells.insert(cell);
		}
	}

	const V2_int minimum{ std::min(first.x, last.x), std::min(first.y, last.y) };
	const V2_int maximum{ std::max(first.x, last.x), std::max(first.y, last.y) };
	const V2_float grid{ ActiveGridSize(scene) };
	Tilemap target{ layer->kind == SceneLayerKind::Tile ? ResolveTargetTilemap(scene) : Tilemap{} };
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	for (V2_int cell : cells) {
		if (!CoveragePass(cell, minimum, maximum)) {
			continue;
		}
		if (target && recipe_.avoid_exclusion_mask && target.IsExcluded(cell)) {
			continue;
		}

		const V2_float cell_min{ ActiveCellToWorld(scene, cell) };
		const V2_float cell_max{ cell_min + grid };
		const V2_float cell_center{ cell_min + grid * 0.5f };
		draw->AddRect(
			ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)),
			ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)), kPreview, 0.0f, 0, 2.0f
		);

		if (recipe_.show_noise_preview && recipe_.source_kind == PaintSourceKind::Noise) {
			const float value{ NoiseValue(cell_center, recipe_.noise) };
			const int shade{ static_cast<int>(std::round(value * 255.0f)) };
			const int alpha{ static_cast<int>(
				std::round(std::clamp(recipe_.noise_preview_alpha, 0.0f, 1.0f) * 255.0f)
			) };
			draw->AddRectFilled(
				ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)),
				ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)),
				IM_COL32(shade, shade, shade, alpha)
			);
		}
		if (!recipe_.show_generated_preview) {
			continue;
		}

		std::optional<PaintTileSource> tile;
		std::optional<PrefabKey> prefab;
		Origin origin{ layer->kind == SceneLayerKind::Tile ? recipe_.tile_origin
														   : recipe_.entity_origin };
		switch (recipe_.source_kind) {
			case PaintSourceKind::Single:
				if (layer->kind == SceneLayerKind::Tile && tile_source_) {
					tile = tile_source_;
				} else if (layer->kind == SceneLayerKind::Entity && prefab_source_) {
					prefab = prefab_source_;
				}
				break;
			case PaintSourceKind::WeightedSet:
				if (layer->kind == SceneLayerKind::Tile) {
					if (const auto* set{ FindWeightedTileSet(recipe_.weighted_tile_set_name) }) {
						const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
							return e.weight;
						}) };
						if (i) {
							tile = set->entries[*i].source;
						}
					}
				} else {
					if (const auto* set{
							FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) }) {
						const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
							return e.weight;
						}) };
						if (i) {
							prefab = set->entries[*i].prefab;
						}
					}
				}
				break;
			case PaintSourceKind::Checkerboard:
				if ((cell.x + cell.y) & 1) {
					tile   = recipe_.checker_tile;
					prefab = recipe_.checker_prefab;
				} else {
					if (tile_source_) {
						tile = tile_source_;
					}
					if (prefab_source_) {
						prefab = prefab_source_;
					}
				}
				break;
			case PaintSourceKind::Autotile: {
				if (const auto* rules{ FindAutotileRuleSet(recipe_.autotile_ruleset_name) }) {
					const int index{ AutotileIndex(
						rules->format, [&](V2_int c) { return cells.contains(c); }, cell
					) };
					if (index >= 0 && index < static_cast<int>(rules->tiles.size())) {
						tile = rules->tiles[static_cast<std::size_t>(index)];
					}
				}
				break;
			}
			case PaintSourceKind::Noise:
				if (const PaintNoiseThreshold* region{ ResolveNoiseThreshold(cell_center) }) {
					origin = region->origin;
					if (region->source_kind == PaintSourceKind::WeightedSet) {
						if (layer->kind == SceneLayerKind::Tile) {
							if (const auto* set{
									FindWeightedTileSet(region->weighted_tile_set_name) }) {
								const auto i{ ChooseWeightedIndex(
									set->entries, cell, [](const auto& e) { return e.weight; }
								) };
								if (i) {
									tile = set->entries[*i].source;
								}
							}
						} else {
							if (const auto* set{
									FindWeightedPrefabSet(region->weighted_prefab_set_name) }) {
								const auto i{ ChooseWeightedIndex(
									set->entries, cell, [](const auto& e) { return e.weight; }
								) };
								if (i) {
									prefab = set->entries[*i].prefab;
								}
							}
						}
					} else {
						tile   = region->tile;
						prefab = region->prefab;
					}
				}
				break;
		}

		if (layer->kind == SceneLayerKind::Tile && tile.has_value()) {
			const V2_float visual_size{ tile->pixel_size.IsPositive() ? V2_float{ tile->pixel_size }
																	  : grid };
			if (recipe_.tile_placement == PaintTilePlacementMode::Tile) {
				const V2_int footprint{
					std::max(
						1, static_cast<int>(std::ceil(visual_size.x / std::max(1.0f, grid.x)))
					),
					std::max(
						1, static_cast<int>(std::ceil(visual_size.y / std::max(1.0f, grid.y)))
					),
				};
				if (FloorMod(cell.x - first.x, footprint.x) != 0 ||
					FloorMod(cell.y - first.y, footprint.y) != 0) {
					continue;
				}
			}
			const auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
				return candidate.kind == AssetKind::Texture &&
					   candidate.key == static_cast<const AssetKey&>(tile->texture);
			}) };
			const V2_float center{ cell_min + GetOffset(origin, visual_size) };
			const V2_float minimum_world{ center - visual_size * 0.5f };
			const V2_float maximum_world{ center + visual_size * 0.5f };
			const ImVec2 p0{ ToImGui(WorldToScreen(minimum_world, frame, presentation_viewport)) };
			const ImVec2 p1{ ToImGui(WorldToScreen(maximum_world, frame, presentation_viewport)) };
			if (record != records.end() && record->preview.has_value()) {
				const auto& uv{ tile->texture_coordinates };
				draw->AddImageQuad(
					static_cast<ImTextureID>(record->preview->texture), p0, { p1.x, p0.y }, p1,
					{ p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]), ToImGui(uv[2]), ToImGui(uv[3]),
					IM_COL32(255, 255, 255, 205)
				);
			} else {
				draw->AddRectFilled(p0, p1, IM_COL32(255, 208, 70, 70));
			}
			draw->AddRect(p0, p1, kPreview, 0.0f, 0, 1.0f);
		} else if (layer->kind == SceneLayerKind::Entity && prefab.has_value()) {
			const ImVec2 p0{ ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)) };
			const ImVec2 p1{ ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)) };
			draw->AddRectFilled(p0, p1, IM_COL32(255, 208, 70, 30));
			draw->AddRect(p0, p1, kPreview, 0.0f, 0, 1.2f);
			const V2_float anchor{ cell_min + GetOffset(origin, grid) };
			draw->AddCircleFilled(
				ToImGui(WorldToScreen(anchor, frame, presentation_viewport)), 3.0f, kPreview, 8
			);
		}
	}
}

void PaintEditor::DrawGrid(
	Scene& scene, ImDrawList* draw, Viewport image_viewport, Viewport presentation_viewport,
	const FrameContext& frame
) const {
	if (!grid_visible_) {
		return;
	}
	const V2_float size{ ActiveGridSize(scene) };
	if (!size.IsPositive()) {
		return;
	}
	const V2_float origin{ ActiveGridOrigin(scene) };
	const V2_float wa{ ScreenToWorld(image_viewport.position, frame, presentation_viewport) };
	const V2_float wb{
		ScreenToWorld(image_viewport.position + image_viewport.size, frame, presentation_viewport)
	};
	const float min_x{ std::min(wa.x, wb.x) };
	const float max_x{ std::max(wa.x, wb.x) };
	const float min_y{ std::min(wa.y, wb.y) };
	const float max_y{ std::max(wa.y, wb.y) };
	const int first_x{ static_cast<int>(std::floor((min_x - origin.x) / size.x)) - 1 };
	const int last_x{ static_cast<int>(std::ceil((max_x - origin.x) / size.x)) + 1 };
	const int first_y{ static_cast<int>(std::floor((min_y - origin.y) / size.y)) - 1 };
	const int last_y{ static_cast<int>(std::ceil((max_y - origin.y) / size.y)) + 1 };
	if (last_x - first_x > 2048 || last_y - first_y > 2048) {
		return;
	}

	const ImU32 minor{ ImGui::GetColorU32(
		ImVec4{ grid_minor_color_[0], grid_minor_color_[1], grid_minor_color_[2],
				grid_minor_color_[3] }
	) };
	const ImU32 major_color{ ImGui::GetColorU32(
		ImVec4{ grid_major_color_[0], grid_major_color_[1], grid_major_color_[2],
				grid_major_color_[3] }
	) };
	for (int x{ first_x }; x <= last_x; ++x) {
		const float world_x{ origin.x + static_cast<float>(x) * size.x };
		const bool major{ x % std::max(1, grid_major_every_) == 0 };
		draw->AddLine(
			ToImGui(WorldToScreen({ world_x, min_y }, frame, presentation_viewport)),
			ToImGui(WorldToScreen({ world_x, max_y }, frame, presentation_viewport)),
			major ? major_color : minor, major ? grid_major_thickness_ : grid_minor_thickness_
		);
	}
	for (int y{ first_y }; y <= last_y; ++y) {
		const float world_y{ origin.y + static_cast<float>(y) * size.y };
		const bool major{ y % std::max(1, grid_major_every_) == 0 };
		draw->AddLine(
			ToImGui(WorldToScreen({ min_x, world_y }, frame, presentation_viewport)),
			ToImGui(WorldToScreen({ max_x, world_y }, frame, presentation_viewport)),
			major ? major_color : minor, major ? grid_major_thickness_ : grid_minor_thickness_
		);
	}
}

void PaintEditor::DrawSelectionOverlay(
	Scene& scene, ImDrawList* draw, Viewport presentation_viewport, const FrameContext& frame
) const {
	for (UUID uuid : selected_entities_) {
		Entity entity{ scene.GetEntity(uuid) };
		if (!entity) {
			continue;
		}
		const auto bounds{ EntitySelectionBounds(entity) };
		if (!bounds.has_value()) {
			continue;
		}
		const ImVec2 p0{ ToImGui(WorldToScreen(bounds->min, frame, presentation_viewport)) };
		const ImVec2 p1{ ToImGui(WorldToScreen(bounds->max, frame, presentation_viewport)) };
		draw->AddRectFilled(p0, p1, IM_COL32(78, 190, 255, 30));
		draw->AddRect(p0, p1, kSelection, 0.0f, 0, 2.0f);
	}

	if (selected_tilemap_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_tilemap_) };
		if (entity && IsTilemap(entity)) {
			Tilemap map{ entity };
			for (V2_int cell : selected_tile_cells_) {
				const TilemapTile* tile{ map.FindTile(cell) };
				if (!tile) {
					continue;
				}
				const PaintSelectionRect bounds{ TileSelectionBounds(map, *tile) };
				const ImVec2 p0{ ToImGui(WorldToScreen(bounds.min, frame, presentation_viewport)) };
				const ImVec2 p1{ ToImGui(WorldToScreen(bounds.max, frame, presentation_viewport)) };
				draw->AddRectFilled(p0, p1, IM_COL32(78, 190, 255, 30));
				draw->AddRect(p0, p1, kSelection, 0.0f, 0, 2.0f);
			}
		}
	}

	if (!selection_drag_active_ || select_mode_ != PaintSelectMode::ClickMarquee) {
		return;
	}

	V2_float current{
		ScreenToWorld(FromImGui(ImGui::GetIO().MousePos), frame, presentation_viewport)
	};
	if (ImGui::GetIO().KeyShift) {
		current = ConstrainSquareDrag(selection_drag_start_, current);
	}

	const V2_float start_screen{ WorldToScreen(selection_drag_start_, frame, presentation_viewport) };
	const V2_float current_screen{ WorldToScreen(current, frame, presentation_viewport) };
	const V2_float screen_delta{ current_screen - start_screen };
	if (std::sqrt(screen_delta.x * screen_delta.x + screen_delta.y * screen_delta.y) < 4.0f) {
		return;
	}

	const PaintSelectionRect bounds{ MakeSelectionRect(selection_drag_start_, current) };
	draw->AddRect(
		ToImGui(WorldToScreen(bounds.min, frame, presentation_viewport)),
		ToImGui(WorldToScreen(bounds.max, frame, presentation_viewport)),
		IM_COL32(255, 255, 255, 204), 0.0f, 0, 2.0f
	);
}

void PaintEditor::DrawToolPreview(
	Scene& scene, ImDrawList* draw, V2_float mouse_world, Viewport presentation_viewport,
	const FrameContext& frame
) const {
	const SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer) {
		return;
	}

	const bool mask_operation{ layer->kind == SceneLayerKind::Tile &&
							   recipe_.operation == PaintBrushOperation::ExclusionMask };
	const ImU32 color{ layer->locked	? kLockedPreview
					   : mask_operation ? IM_COL32(255, 72, 72, 220)
										: kPreview };

	const V2_float size{ ActiveGridSize(scene) };
	const V2_int cell{ WorldToActiveCell(scene, mouse_world) };

	auto draw_cell = [&](V2_int c) {
		const V2_float mn{ ActiveCellToWorld(scene, c) };
		const V2_float mx{ mn + size };

		if (mask_operation) {
			draw->AddRectFilled(
				ToImGui(WorldToScreen(mn, frame, presentation_viewport)),
				ToImGui(WorldToScreen(mx, frame, presentation_viewport)), IM_COL32(255, 32, 32, 42)
			);
		}

		draw->AddRect(
			ToImGui(WorldToScreen(mn, frame, presentation_viewport)),
			ToImGui(WorldToScreen(mx, frame, presentation_viewport)), color, 0.0f, 0, 1.5f
		);
	};

	if (tool_ == PaintTool::Select && select_mode_ == PaintSelectMode::Brush) {
		for (V2_int c : SelectionBrushCells(cell)) {
			draw_cell(c);
		}
	} else if (tool_ == PaintTool::Brush || tool_ == PaintTool::Erase) {
		for (V2_int c : BrushCells(cell)) {
			draw_cell(c);
		}
	} else if ((tool_ == PaintTool::Line || tool_ == PaintTool::Rectangle) && stroke_.active) {
		const V2_int start{ WorldToActiveCell(scene, stroke_.start_world) };
		const auto cells{ tool_ == PaintTool::Line ? LineCells(start, cell)
												   : RectangleCells(start, cell) };

		for (V2_int c : cells) {
			draw_cell(c);
		}
	} else if (
		tool_ != PaintTool::Select && tool_ != PaintTool::Move && tool_ != PaintTool::Eyedropper
	) {
		draw_cell(cell);
	}
}

void PaintEditor::BeginStroke(Scene& scene, V2_float world) {
	stroke_				  = {};
	stroke_.active		  = true;
	stroke_.start_world	  = world;
	stroke_.current_world = world;
	stroke_.last_cell	  = WorldToActiveCell(scene, world);
	stroke_.has_last_cell = true;
	if (tool_ == PaintTool::Brush && recipe_.commit_mode == PaintCommitMode::KeepGenerator) {
		stroke_.generator_points.push_back(world);
	}

	if (const SceneLayer* layer{ ResolveActiveLayer(scene) };
		layer && layer->kind == SceneLayerKind::Tile) {
		if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			stroke_.tilemap		   = map.Get<UUID>();
			stroke_.tilemap_before = map.GetData();
		}
	}
}

void PaintEditor::ApplyTileAt(EditorContext& ctx, Scene&, Tilemap tilemap, V2_int cell) {
	if (!tilemap) {
		return;
	}
	if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
		tilemap.SetExcluded(cell, true);
		return;
	}
	if (recipe_.avoid_exclusion_mask && tilemap.IsExcluded(cell)) {
		return;
	}
	if (recipe_.coverage == PaintCoverageMode::RandomDensity && Hash01(cell) > recipe_.density) {
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Autotile) {
		const PaintAutotileRuleSet* rules{ FindAutotileRuleSet(recipe_.autotile_ruleset_name) };
		if (!rules || rules->id == 0 || rules->tiles.empty()) {
			return;
		}
		tilemap.SetTerrainRuleset(cell, rules->id);
		RecomputeAutotileAround(tilemap, cell, *rules);
		return;
	}

	// Painting an ordinary tile replaces logical terrain at that cell. Recompute its old
	// neighborhood first so adjacent derived terrain variants remain correct.
	if (const auto old_ruleset{ tilemap.GetTerrainRuleset(cell) }) {
		tilemap.SetTerrainRuleset(cell, std::nullopt);
		if (const PaintAutotileRuleSet* old_rules{ FindAutotileRuleSet(*old_ruleset) }) {
			RecomputeAutotileAround(tilemap, cell, *old_rules);
		}
	}

	PaintTileSource source{};
	Origin origin{ recipe_.tile_origin };
	switch (recipe_.source_kind) {
		case PaintSourceKind::Single:	   source = tile_source_; break;
		case PaintSourceKind::WeightedSet: {
			if (const auto* set{ FindWeightedTileSet(recipe_.weighted_tile_set_name) }) {
				const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
					return e.weight;
				}) };
				if (i) {
					source = set->entries[*i].source;
				}
			}
			break;
		}
		case PaintSourceKind::Checkerboard:
			source = ((cell.x + cell.y) & 1) && recipe_.checker_tile ? *recipe_.checker_tile
																	 : tile_source_;
			break;
		case PaintSourceKind::Noise: {
			const V2_float sample_world{ tilemap.CellToWorld(cell) +
										 tilemap.GetData().cell_size * 0.5f };
			const PaintNoiseThreshold* region{ ResolveNoiseThreshold(sample_world) };
			if (!region) {
				return;
			}
			origin = region->origin;
			if (region->source_kind == PaintSourceKind::WeightedSet) {
				if (const auto* set{ FindWeightedTileSet(region->weighted_tile_set_name) }) {
					const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
						return e.weight;
					}) };
					if (i) {
						source = set->entries[*i].source;
					}
				}
			} else if (region->tile.has_value()) {
				source = *region->tile;
			}
			break;
		}
		case PaintSourceKind::Autotile: break;
	}
	if (!source) {
		return;
	}

	if (recipe_.tile_placement == PaintTilePlacementMode::Tile) {
		const V2_float cell_size{ tilemap.GetData().cell_size };
		const V2_int footprint{
			std::max(
				1, static_cast<int>(std::ceil(
					   static_cast<float>(source.pixel_size.x) / std::max(1.0f, cell_size.x)
				   ))
			),
			std::max(
				1, static_cast<int>(std::ceil(
					   static_cast<float>(source.pixel_size.y) / std::max(1.0f, cell_size.y)
				   ))
			)
		};
		const V2_int lattice_origin{ stroke_.active ? tilemap.WorldToCell(stroke_.start_world)
													: cell };
		if (FloorMod(cell.x - lattice_origin.x, footprint.x) != 0 ||
			FloorMod(cell.y - lattice_origin.y, footprint.y) != 0) {
			return;
		}
	}

	const TilemapTile* existing{ tilemap.FindTile(cell) };
	if (recipe_.operation == PaintBrushOperation::Paint && existing) {
		return;
	}
	if (recipe_.operation == PaintBrushOperation::Replace && !existing) {
		return;
	}

	tilemap.SetTile(
		TilemapTile{
			.coordinate			 = cell,
			.texture			 = source.texture,
			.texture_coordinates = source.texture_coordinates,
			.pixel_size			 = source.pixel_size,
			.origin				 = origin,
			.offset				 = {},
			.tint				 = color::White,
			.terrain_ruleset_id	 = std::nullopt,
		}
	);
	static_cast<void>(ctx);
}

void PaintEditor::ApplyEntityAt(EditorContext& ctx, Scene& scene, V2_float world) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->kind != SceneLayerKind::Entity || layer->locked) {
		return;
	}

	const V2_int cell{ WorldToActiveCell(scene, world) };
	if (recipe_.coverage == PaintCoverageMode::RandomDensity && Hash01(cell) > recipe_.density) {
		return;
	}

	PrefabKey source{};
	Origin origin{ recipe_.entity_origin };
	switch (recipe_.source_kind) {
		case PaintSourceKind::Single:	   source = prefab_source_; break;
		case PaintSourceKind::WeightedSet: {
			if (const auto* set{ FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) }) {
				const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
					return e.weight;
				}) };
				if (i) {
					source = set->entries[*i].prefab;
				}
			}
			break;
		}
		case PaintSourceKind::Checkerboard:
			source = ((cell.x + cell.y) & 1) && recipe_.checker_prefab ? *recipe_.checker_prefab
																	   : prefab_source_;
			break;
		case PaintSourceKind::Autotile: return;
		case PaintSourceKind::Noise:	{
			const V2_float sample_world{ ActiveCellToWorld(scene, cell) +
										 ActiveGridSize(scene) * 0.5f };
			const PaintNoiseThreshold* region{ ResolveNoiseThreshold(sample_world) };
			if (!region) {
				return;
			}
			origin = region->origin;
			if (region->source_kind == PaintSourceKind::WeightedSet) {
				if (const auto* set{ FindWeightedPrefabSet(region->weighted_prefab_set_name) }) {
					const auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
						return e.weight;
					}) };
					if (i) {
						source = set->entries[*i].prefab;
					}
				}
			} else if (region->prefab.has_value()) {
				source = *region->prefab;
			}
			break;
		}
	}
	if (!source) {
		return;
	}

	// Prefab source browsers enumerate the project catalog, but Scene::CreatePrefab() requires
	// the prefab object to be resident. A prefab can therefore be selectable after reopening a
	// project without having been loaded yet. Resolve that catalog entry synchronously before
	// instantiation so painting works independently of scene preload dependencies.
	auto& assets{ ctx.editor.GetAssetManager() };
	if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(source)) {
		const auto catalog{ assets.GetCatalogAsset(source, AssetKind::Prefab) };
		if (!catalog.has_value()) {
			return;
		}
		assets.Load(source, catalog->source_path);
	}
	if (!::ptgn::impl::AssetAccessor{ assets }.Has<Prefab>(source)) {
		return;
	}

	std::vector<Entity> occupied;
	for (Entity root : scene.GetLayers().GetRootEntities(scene, layer->id)) {
		if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
			!root.Has<Transform>()) {
			continue;
		}
		if (WorldToActiveCell(scene, GetWorldPosition(root)) == cell) {
			occupied.emplace_back(root);
		}
	}
	if (recipe_.operation == PaintBrushOperation::Paint && !occupied.empty()) {
		return;
	}
	if (recipe_.operation == PaintBrushOperation::Replace) {
		for (Entity entity : occupied) {
			const UUID uuid{ entity.Get<UUID>() };
			const auto created_it{ std::ranges::find(stroke_.entities.created_roots, uuid) };
			if (created_it != stroke_.entities.created_roots.end()) {
				stroke_.entities.created_roots.erase(created_it);
			} else {
				stroke_.entities.deleted_roots.push_back(
					DeletedEntity{ .entity = SerializeEntity(entity), .layer = layer->id }
				);
			}
			entity.Destroy();
		}
		if (!occupied.empty()) {
			scene.Refresh();
		}
	}

	world = ActiveCellToWorld(scene, cell) + GetOffset(origin, ActiveGridSize(scene));

	if (recipe_.coverage != PaintCoverageMode::Solid && recipe_.min_spacing > 0.0f) {
		for (Entity root : scene.GetLayers().GetRootEntities(scene, layer->id)) {
			if (!root.Has<Transform>() || IsProtectedSceneEntity(scene, root) ||
				IsPaintGenerator(root)) {
				continue;
			}
			const V2_float delta{ GetWorldPosition(root) - world };
			if (std::sqrt(delta.x * delta.x + delta.y * delta.y) < recipe_.min_spacing) {
				return;
			}
		}
	}

	auto prefab_asset{ ::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(source) };
	Entity entity{ InstantiatePrefab(
		scene, prefab_asset.get(),
		recipe_.link_prefab_instances ? PrefabInstantiationMode::Linked
									  : PrefabInstantiationMode::Baked
	) };
	if (!entity) {
		return;
	}
	if (!scene.GetLayers().Assign(entity, layer->id, true)) {
		entity.Destroy();
		scene.Refresh();
		return;
	}
	Transform world_transform{ GetWorldTransform(entity) };
	world_transform.position = world;
	SetWorldTransform(entity, world_transform);
	const float selector{ Hash01(cell) };
	if (recipe_.random_rotation) {
		SetRotation(
			entity, Degrees{ recipe_.rotation_min +
							 (recipe_.rotation_max - recipe_.rotation_min) * selector }
		);
	}
	if (recipe_.random_scale) {
		SetScale(
			entity,
			recipe_.scale_min + (recipe_.scale_max - recipe_.scale_min) * Hash01({ cell.y, cell.x })
		);
	}
	stroke_.entities.created_roots.push_back(entity.Get<UUID>());
}

void PaintEditor::ApplyAt(EditorContext& ctx, Scene& scene, V2_float world) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}
	if (layer->kind == SceneLayerKind::Tile) {
		if (Tilemap tilemap{ ResolveTargetTilemap(scene) }) {
			const V2_int cell{ tilemap.WorldToCell(world) };
			if (stroke_.touched_cells.insert(cell).second || tool_ == PaintTool::Pencil) {
				ApplyTileAt(ctx, scene, tilemap, cell);
			}
		}
	} else {
		const V2_int cell{ WorldToActiveCell(scene, world) };
		if (stroke_.touched_cells.insert(cell).second || tool_ == PaintTool::Pencil) {
			ApplyEntityAt(ctx, scene, world);
		}
	}
}

void PaintEditor::EraseAt(EditorContext&, Scene& scene, V2_float world) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}
	const V2_int center{ WorldToActiveCell(scene, world) };
	const auto cells{ BrushCells(center) };

	if (layer->kind == SceneLayerKind::Tile) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		for (V2_int cell : cells) {
			if (!stroke_.touched_cells.insert(cell).second) {
				continue;
			}
			if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
				map.SetExcluded(cell, false);
				continue;
			}
			if (const auto terrain{ map.GetTerrainRuleset(cell) }) {
				map.SetTerrainRuleset(cell, std::nullopt);
				if (const PaintAutotileRuleSet* rules{ FindAutotileRuleSet(*terrain) }) {
					RecomputeAutotileAround(map, cell, *rules);
				}
			} else {
				map.EraseTile(cell);
			}
		}
		return;
	}

	std::vector<Entity> erase;
	for (Entity root : scene.GetLayers().GetRootEntities(scene, layer->id)) {
		if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
			!root.Has<Transform>()) {
			continue;
		}
		const V2_int entity_cell{ WorldToActiveCell(scene, GetWorldPosition(root)) };
		if (std::ranges::contains(cells, entity_cell)) {
			erase.emplace_back(root);
		}
	}
	for (Entity entity : erase) {
		const UUID uuid{ entity.Get<UUID>() };
		const auto created_it{ std::ranges::find(stroke_.entities.created_roots, uuid) };
		if (created_it != stroke_.entities.created_roots.end()) {
			stroke_.entities.created_roots.erase(created_it);
		} else {
			stroke_.entities.deleted_roots.push_back(
				DeletedEntity{
					.entity = SerializeEntity(entity),
					.layer	= layer->id,
				}
			);
		}
		entity.Destroy();
	}
	if (!erase.empty()) {
		scene.Refresh();
	}
}

void PaintEditor::FillAt(EditorContext& ctx, Scene& scene, V2_float world) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}

	if (layer->kind == SceneLayerKind::Tile) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		const V2_int start{ map.WorldToCell(world) };
		const TilemapTile* start_tile{ map.FindTile(start) };
		const std::optional<TextureKey> match_texture{
			start_tile ? std::optional<TextureKey>{ start_tile->texture } : std::nullopt
		};
		const auto match_uv{ start_tile ? std::optional{ start_tile->texture_coordinates }
										: std::nullopt };

		std::vector<V2_int> queue{ start };
		std::unordered_set<V2_int> visited;
		visited.reserve(1024);
		for (std::size_t head{}; head < queue.size() && visited.size() < kMaxFloodCells; ++head) {
			const V2_int cell{ queue[head] };
			if (!visited.insert(cell).second) {
				continue;
			}
			const TilemapTile* current{ map.FindTile(cell) };
			const bool matches{ match_texture.has_value()
									? current && current->texture == *match_texture &&
										  current->texture_coordinates == *match_uv
									: current == nullptr };
			if (!matches) {
				continue;
			}
			ApplyTileAt(ctx, scene, map, cell);
			queue.emplace_back(cell + V2_int{ 1, 0 });
			queue.emplace_back(cell + V2_int{ -1, 0 });
			queue.emplace_back(cell + V2_int{ 0, 1 });
			queue.emplace_back(cell + V2_int{ 0, -1 });
		}
		return;
	}

	// Entity fill is deliberately bounded to the visible scene-camera rectangle so an empty
	// connected region can never expand forever. It fills either the connected empty region or,
	// with Replace, the connected occupied region starting under the cursor.
	const auto vertices{ scene.ctx().camera.GetWorldVertices() };
	V2_float minimum{ vertices.front() };
	V2_float maximum{ vertices.front() };
	for (const V2_float vertex : vertices) {
		minimum = Min(minimum, vertex);
		maximum = Max(maximum, vertex);
	}
	const V2_int min_cell{ WorldToActiveCell(scene, minimum) - V2_int{ 1, 1 } };
	const V2_int max_cell{ WorldToActiveCell(scene, maximum) + V2_int{ 1, 1 } };
	const V2_int start{ WorldToActiveCell(scene, world) };

	std::unordered_set<V2_int> occupied;
	for (Entity root : scene.GetLayers().GetRootEntities(scene, layer->id)) {
		if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
			IsTilemap(root) || !root.Has<Transform>()) {
			continue;
		}
		occupied.insert(WorldToActiveCell(scene, GetWorldPosition(root)));
	}
	const bool start_occupied{ occupied.contains(start) };
	if (start_occupied && recipe_.operation == PaintBrushOperation::Paint) {
		return;
	}

	std::vector<V2_int> queue{ start };
	std::unordered_set<V2_int> visited;
	visited.reserve(1024);
	for (std::size_t head{}; head < queue.size() && visited.size() < kMaxFloodCells; ++head) {
		const V2_int cell{ queue[head] };
		if (cell.x < min_cell.x || cell.x > max_cell.x || cell.y < min_cell.y ||
			cell.y > max_cell.y) {
			continue;
		}
		if (!visited.insert(cell).second) {
			continue;
		}
		if (occupied.contains(cell) != start_occupied) {
			continue;
		}
		ApplyEntityAt(ctx, scene, ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f);
		queue.emplace_back(cell + V2_int{ 1, 0 });
		queue.emplace_back(cell + V2_int{ -1, 0 });
		queue.emplace_back(cell + V2_int{ 0, 1 });
		queue.emplace_back(cell + V2_int{ 0, -1 });
	}
}

void PaintEditor::EyedropAt(EditorContext&, Scene& scene, V2_float world) {
	const SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked || !layer->selectable) {
		return;
	}

	if (layer->kind == SceneLayerKind::Tile) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}

		if (const TilemapTile* tile{ map.FindTile(map.WorldToCell(world)) }) {
			recipe_.source_kind = PaintSourceKind::Single;
			tile_source_ = PaintTileSource{
				.texture = tile->texture,
				.texture_coordinates = tile->texture_coordinates,
				.pixel_size =
					tile->pixel_size.IsPositive()
						? tile->pixel_size
						: V2_int{
							static_cast<int>(map.GetData().cell_size.x),
							static_cast<int>(map.GetData().cell_size.y),
						},
			};
		}
		return;
	}

	const V2_int hovered_cell{ WorldToActiveCell(scene, world) };

	auto roots{ scene.GetLayers().GetRootEntities(scene, layer->id) };

	for (auto it{ roots.rbegin() }; it != roots.rend(); ++it) {
		Entity candidate{ *it };
		if (!candidate || IsProtectedSceneEntity(scene, candidate) || IsPaintGenerator(candidate) ||
			IsTilemap(candidate) || !candidate.Has<Transform>() ||
			WorldToActiveCell(scene, GetWorldPosition(candidate)) != hovered_cell) {
			continue;
		}

		Entity instance_root{ GetPrefabInstanceRoot(candidate) };
		if (!instance_root) {
			// Ordinary/baked entities intentionally do not retain a prefab source identity.
			return;
		}

		const PrefabInstance& link{ instance_root.Get<PrefabInstance>() };
		recipe_.source_kind			  = PaintSourceKind::Single;
		recipe_.link_prefab_instances = true;
		prefab_source_				  = link.prefab;
		return;
	}
}

void PaintEditor::UpdateStroke(EditorContext& ctx, Scene& scene, V2_float world) {
	if (!stroke_.active) {
		return;
	}
	stroke_.current_world = world;

	if (tool_ == PaintTool::Line || tool_ == PaintTool::Rectangle) {
		return;
	}
	if (tool_ == PaintTool::Erase) {
		EraseAt(ctx, scene, world);
		return;
	}

	const V2_int current{ WorldToActiveCell(scene, world) };
	if (!stroke_.has_last_cell) {
		stroke_.last_cell	  = current;
		stroke_.has_last_cell = true;
	}
	const std::vector<V2_int> path{ LineCells(stroke_.last_cell, current) };

	if (tool_ == PaintTool::Brush && recipe_.commit_mode == PaintCommitMode::KeepGenerator &&
		recipe_.operation != PaintBrushOperation::ExclusionMask) {
		for (V2_int cell : path) {
			if (!stroke_.touched_cells.insert(cell).second) {
				continue;
			}
			stroke_.generator_points.push_back(
				ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f
			);
		}
		stroke_.last_cell = current;
		return;
	}

	for (V2_int cell : path) {
		if (tool_ == PaintTool::Brush) {
			for (V2_int brush_cell : BrushCells(cell)) {
				ApplyAt(
					ctx, scene, ActiveCellToWorld(scene, brush_cell) + ActiveGridSize(scene) * 0.5f
				);
			}
		} else {
			ApplyAt(ctx, scene, ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f);
		}
	}
	stroke_.last_cell = current;
}

void PaintEditor::ApplyLine(EditorContext& ctx, Scene& scene, V2_float a, V2_float b) {
	const V2_int first{ WorldToActiveCell(scene, a) };
	const V2_int last{ WorldToActiveCell(scene, b) };
	const auto cells{ LineCells(first, last) };
	for (V2_int cell : cells) {
		const V2_int mn{ std::min(first.x, last.x), std::min(first.y, last.y) };
		const V2_int mx{ std::max(first.x, last.x), std::max(first.y, last.y) };
		if (!CoveragePass(cell, mn, mx)) {
			continue;
		}
		const V2_float world{ ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f };
		const std::size_t created_before{ stroke_.entities.created_roots.size() };
		ApplyAt(ctx, scene, world);
		if (line_align_rotation_ && stroke_.entities.created_roots.size() > created_before) {
			const V2_float delta{ b - a };
			const Radians angle{ std::atan2(delta.y, delta.x) };
			for (std::size_t i{ created_before }; i < stroke_.entities.created_roots.size(); ++i) {
				if (Entity e{ scene.GetEntity(stroke_.entities.created_roots[i]) }) {
					SetRotation(e, angle);
				}
			}
		}
	}
}

void PaintEditor::ApplyRectangle(EditorContext& ctx, Scene& scene, V2_float a, V2_float b) {
	const V2_int first{ WorldToActiveCell(scene, a) };
	const V2_int last{ WorldToActiveCell(scene, b) };
	for (V2_int cell : RectangleCells(first, last)) {
		const V2_int mn{ std::min(first.x, last.x), std::min(first.y, last.y) };
		const V2_int mx{ std::max(first.x, last.x), std::max(first.y, last.y) };
		if (!CoveragePass(cell, mn, mx)) {
			continue;
		}
		ApplyAt(ctx, scene, ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f);
	}
}

void PaintEditor::CreateGeneratorForStroke(
	EditorContext& ctx, Scene& scene, PaintGeneratorGeometry geometry
) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}

	PaintGenerator generator{ CreatePaintGenerator(
		scene, layer->id,
		Tag{ geometry == PaintGeneratorGeometry::BrushStroke ? "Brush Generator"
			 : geometry == PaintGeneratorGeometry::Line		 ? "Line Generator"
															 : "Rectangle Generator" }
	) };
	if (!generator) {
		return;
	}

	auto& data{ generator.GetData() };
	data.geometry	 = geometry;
	data.recipe		 = CaptureGeneratorRecipe(*layer);
	data.grid_size	 = ActiveGridSize(scene);
	data.grid_offset = ActiveGridOrigin(scene);
	data.start		 = stroke_.start_world;
	data.end		 = stroke_.current_world;
	data.brush_shape = brush_shape_ == PaintBrushShape::Circle ? PaintGeneratorBrushShape::Circle
															   : PaintGeneratorBrushShape::Square;
	data.line_thickness		 = line_thickness_;
	data.line_spacing		 = line_spacing_;
	data.area_mode			 = static_cast<PaintGeneratorAreaMode>(area_mode_);
	data.area_thickness		 = area_thickness_;
	data.random_fill_density = recipe_.density;

	if (geometry == PaintGeneratorGeometry::BrushStroke) {
		const std::uint32_t stroke_id{ next_active_brush_stroke_id_++ };
		for (V2_float point : stroke_.generator_points) {
			data.stroke_points.push_back(
				PaintGeneratorStrokePoint{
					.position  = point,
					.diameter  = brush_diameter_,
					.stroke_id = stroke_id,
				}
			);
		}
		if (data.stroke_points.empty()) {
			data.stroke_points.push_back(
				PaintGeneratorStrokePoint{
					.position  = stroke_.start_world,
					.diameter  = brush_diameter_,
					.stroke_id = stroke_id,
				}
			);
		}
	}

	if (layer->kind == SceneLayerKind::Tile) {
		if (Tilemap target{ ResolveTargetTilemap(scene) };
			target && !generator.SetTargetTilemap(target)) {
			generator.Destroy();
			scene.Refresh();
			return;
		}
	}

	scene.Refresh();

	if (geometry == PaintGeneratorGeometry::BrushStroke) {
		// The first Brush stroke starts a live generator. Do not push Create Entity yet:
		// Cancel Generator must be able to discard the complete authoring session without
		// leaving a stale global undo entry.
		active_generator_before_selection_ = ctx.local.selection;
		active_brush_generator_			   = generator.Get<UUID>();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(generator, false);
		return;
	}

	Entity recorded{ ctx.commands.RecordCreatedEntity(generator, ctx.local.selection) };
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(recorded, false);
}

bool PaintEditor::IsActiveBrushGenerator(Entity generator) const {
	return generator && active_brush_generator_.has_value() && generator.Has<UUID>() &&
		   generator.Get<UUID>() == *active_brush_generator_;
}

void PaintEditor::AppendBrushStrokeToGenerator(EditorContext& ctx, Scene& scene) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}

	Entity entity{ active_brush_generator_.has_value() ? scene.GetEntity(*active_brush_generator_)
													   : Entity{} };

	if (!entity || !IsPaintGenerator(entity) ||
		scene.GetLayers().GetLayerId(entity) != std::optional<SceneLayerId>{ layer->id }) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		next_active_brush_stroke_id_ = 1;
		CreateGeneratorForStroke(ctx, scene, PaintGeneratorGeometry::BrushStroke);
		return;
	}

	PaintGenerator generator{ entity };
	auto& data{ generator.GetData() };

	// The recipe remains live for the unfinished generator, matching the sandbox
	// workflow. Stroke identity is NOT inferred from diameter, so equal diameters
	// never merge two authored strokes.
	data.recipe		 = CaptureGeneratorRecipe(*layer);
	data.brush_shape = brush_shape_ == PaintBrushShape::Circle ? PaintGeneratorBrushShape::Circle
															   : PaintGeneratorBrushShape::Square;

	const std::uint32_t stroke_id{ next_active_brush_stroke_id_++ };
	if (stroke_.generator_points.empty()) {
		data.stroke_points.push_back(
			PaintGeneratorStrokePoint{
				.position  = stroke_.start_world,
				.diameter  = brush_diameter_,
				.stroke_id = stroke_id,
			}
		);
	} else {
		for (V2_float point : stroke_.generator_points) {
			data.stroke_points.push_back(
				PaintGeneratorStrokePoint{
					.position  = point,
					.diameter  = brush_diameter_,
					.stroke_id = stroke_id,
				}
			);
		}
	}

	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(entity, false);
	ctx.local.state.is_dirty = true;
}

void PaintEditor::FinishActiveBrushGenerator(EditorContext& ctx, Scene& scene) {
	if (!active_brush_generator_.has_value()) {
		return;
	}

	Entity generator{ scene.GetEntity(*active_brush_generator_) };
	if (!generator || !IsPaintGenerator(generator)) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		next_active_brush_stroke_id_ = 1;
		return;
	}

	const EditorSelection before{
		active_generator_before_selection_.value_or(ctx.local.selection)
	};

	Entity recorded{ ctx.commands.RecordCreatedEntity(generator, before) };
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(recorded, false);

	active_brush_generator_.reset();
	active_generator_before_selection_.reset();
	next_active_brush_stroke_id_ = 1;
	ctx.local.state.is_dirty	 = true;
}

void PaintEditor::CancelActiveBrushGenerator(EditorContext& ctx, Scene& scene) {
	if (!active_brush_generator_.has_value()) {
		return;
	}

	if (Entity generator{ scene.GetEntity(*active_brush_generator_) }) {
		generator.Destroy();
		scene.Refresh();
	}

	if (active_generator_before_selection_.has_value()) {
		ApplyEditorSelection(ctx, *active_generator_before_selection_);
	} else {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
	}

	active_brush_generator_.reset();
	active_generator_before_selection_.reset();
	next_active_brush_stroke_id_ = 1;
	stroke_						 = {};
}

void PaintEditor::UndoActiveBrushStroke(EditorContext& ctx, Scene& scene) {
	if (!active_brush_generator_.has_value()) {
		return;
	}

	Entity entity{ scene.GetEntity(*active_brush_generator_) };
	if (!entity || !IsPaintGenerator(entity)) {
		return;
	}

	auto& points{ PaintGenerator{ entity }.GetData().stroke_points };
	if (points.empty()) {
		CancelActiveBrushGenerator(ctx, scene);
		return;
	}

	std::uint32_t last_id{};
	for (const auto& point : points) {
		last_id = std::max(last_id, point.stroke_id);
	}

	std::erase_if(points, [last_id](const PaintGeneratorStrokePoint& point) {
		return point.stroke_id == last_id;
	});

	if (points.empty()) {
		CancelActiveBrushGenerator(ctx, scene);
		return;
	}

	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(entity, false);
	ctx.local.state.is_dirty = true;
}

void PaintEditor::CreateInfiniteGenerator(EditorContext& ctx, Scene& scene) {
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer || layer->locked) {
		return;
	}
	PaintGenerator generator{ CreatePaintGenerator(scene, layer->id, Tag{ "Infinite Generator" }) };
	if (!generator) {
		return;
	}
	auto& data{ generator.GetData() };
	data.geometry	 = PaintGeneratorGeometry::Infinite;
	data.recipe		 = CaptureGeneratorRecipe(*layer);
	data.grid_size	 = ActiveGridSize(scene);
	data.grid_offset = ActiveGridOrigin(scene);
	if (layer->kind == SceneLayerKind::Tile) {
		if (Tilemap target{ ResolveTargetTilemap(scene) };
			target && !generator.SetTargetTilemap(target)) {
			generator.Destroy();
			scene.Refresh();
			return;
		}
	}
	scene.Refresh();
	Entity recorded{ ctx.commands.RecordCreatedEntity(generator, ctx.local.selection) };
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(recorded, false);
}

void PaintEditor::CommitTileStroke(EditorContext& ctx, Scene& scene) {
	if (!stroke_.tilemap.has_value() || !stroke_.tilemap_before.has_value()) {
		return;
	}
	Entity entity{ scene.GetEntity(*stroke_.tilemap) };
	if (!entity || !entity.Has<::ptgn::impl::TilemapData>()) {
		return;
	}
	const ::ptgn::impl::TilemapData before{ *stroke_.tilemap_before };
	const ::ptgn::impl::TilemapData after{ entity.Get<::ptgn::impl::TilemapData>() };
	json before_json = before;
	json after_json	 = after;
	if (before_json == after_json) {
		return;
	}
	Scene* scene_ptr{ &scene };
	const UUID uuid{ *stroke_.tilemap };
	ctx.undo.PushApplied(
		"Paint Tiles",
		[scene_ptr, uuid, before]() {
			if (Entity e{ scene_ptr->GetEntity(uuid) }; e && e.Has<::ptgn::impl::TilemapData>()) {
				e.Get<::ptgn::impl::TilemapData>() = before;
			}
		},
		[scene_ptr, uuid, after]() {
			if (Entity e{ scene_ptr->GetEntity(uuid) }; e && e.Has<::ptgn::impl::TilemapData>()) {
				e.Get<::ptgn::impl::TilemapData>() = after;
			}
		}
	);
}

void PaintEditor::CommitEntityStroke(EditorContext& ctx, Scene& scene) {
	std::vector<DeletedEntity> created;
	created.reserve(stroke_.entities.created_roots.size());
	for (UUID uuid : stroke_.entities.created_roots) {
		if (Entity entity{ scene.GetEntity(uuid) }) {
			created.push_back(
				DeletedEntity{ .entity = SerializeEntity(entity), .layer = GetActiveLayer(scene) }
			);
		}
	}
	const auto deleted{ stroke_.entities.deleted_roots };
	if (created.empty() && deleted.empty()) {
		return;
	}
	Scene* scene_ptr{ &scene };

	auto apply = [scene_ptr](
					 const std::vector<DeletedEntity>& remove,
					 const std::vector<DeletedEntity>& restore
				 ) {
		for (const auto& item : remove) {
			if (item.entity.uuid.has_value()) {
				if (Entity entity{ scene_ptr->GetEntity(*item.entity.uuid) }) {
					entity.Destroy();
				}
			}
		}
		scene_ptr->Refresh();
		for (const auto& item : restore) {
			static_cast<void>(RestoreEntityTree(*scene_ptr, item.entity, item.layer));
		}
		scene_ptr->Refresh();
	};

	ctx.undo.PushApplied(
		"Paint Entities", [apply, created, deleted]() mutable { apply(created, deleted); },
		[apply, created, deleted]() mutable { apply(deleted, created); }
	);
}

void PaintEditor::EndStroke(EditorContext& ctx, Scene& scene, V2_float world) {
	if (!stroke_.active) {
		return;
	}
	stroke_.current_world = world;
	const bool keep_generator{ recipe_.commit_mode == PaintCommitMode::KeepGenerator &&
							   recipe_.operation != PaintBrushOperation::ExclusionMask &&
							   (tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
								tool_ == PaintTool::Rectangle) };
	if (keep_generator) {
		if (tool_ == PaintTool::Brush) {
			AppendBrushStrokeToGenerator(ctx, scene);
		} else {
			CreateGeneratorForStroke(
				ctx, scene,
				tool_ == PaintTool::Line ? PaintGeneratorGeometry::Line
										 : PaintGeneratorGeometry::Rectangle
			);
		}
		stroke_ = {};
		return;
	}
	if (tool_ == PaintTool::Line) {
		ApplyLine(ctx, scene, stroke_.start_world, world);
	}
	if (tool_ == PaintTool::Rectangle) {
		ApplyRectangle(ctx, scene, stroke_.start_world, world);
	}
	if (const SceneLayer* layer{ ResolveActiveLayer(scene) };
		layer && layer->kind == SceneLayerKind::Tile) {
		CommitTileStroke(ctx, scene);
	} else {
		CommitEntityStroke(ctx, scene);
	}
	stroke_ = {};
}

void PaintEditor::CancelStroke(Scene& scene) {
	if (!stroke_.active) {
		return;
	}
	if (stroke_.tilemap.has_value() && stroke_.tilemap_before.has_value()) {
		if (Entity e{ scene.GetEntity(*stroke_.tilemap) };
			e && e.Has<::ptgn::impl::TilemapData>()) {
			e.Get<::ptgn::impl::TilemapData>() = *stroke_.tilemap_before;
		}
	}
	for (UUID uuid : stroke_.entities.created_roots) {
		if (Entity e{ scene.GetEntity(uuid) }) {
			e.Destroy();
		}
	}
	scene.Refresh();
	for (const auto& deleted : stroke_.entities.deleted_roots) {
		static_cast<void>(RestoreEntityTree(scene, deleted.entity, deleted.layer));
	}
	scene.Refresh();
	stroke_ = {};
}

void PaintEditor::SelectClick(
	EditorContext& ctx, Scene& scene, V2_float world, bool additive, bool toggle
) {
	// Selection is scene-wide, not constrained by the current authoring layer.
	// Walk the layer stack from front/top to back/bottom and stop at the first
	// selectable object under the cursor. The active paint layer is deliberately
	// left unchanged: it controls where new content is authored, not what may be
	// selected.
	struct Hit {
		enum class Kind {
			None,
			Entity,
			Tile,
			Generator,
		};

		Kind kind{ Kind::None };
		Entity entity{};
		Tilemap tilemap{};
		V2_int tile_cell{};
	};

	Hit hit{};
	const auto& layers{ scene.GetLayers().GetLayers() };

	for (auto layer_it{ layers.rbegin() };
		 layer_it != layers.rend() && hit.kind == Hit::Kind::None;
		 ++layer_it) {
		const SceneLayer& layer{ *layer_it };
		if (!layer.visible || layer.locked || !layer.selectable) {
			continue;
		}

		auto roots{ scene.GetLayers().GetRootEntities(scene, layer.id) };
		SortByLocalDepth(roots);

		if (layer.kind == SceneLayerKind::Entity) {
			// Manually authored entities sit in front of generators on the same
			// layer, just as they do in the sandbox demo.
			for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
				Entity root{ *root_it };
				if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
					IsProtectedSceneEntity(scene, root)) {
					continue;
				}

				const auto bounds{ EntitySelectionBounds(root) };
				if (bounds.has_value() && Contains(*bounds, world)) {
					hit.kind = Hit::Kind::Entity;
					hit.entity = root;
					break;
				}
			}
		} else if (layer.kind == SceneLayerKind::Tile) {
			// A tile on a higher layer blocks objects beneath it. Walk tilemaps
			// and their authored tiles back-to-front so overlapping visual bounds
			// pick the visually upper tile.
			for (auto root_it{ roots.rbegin() };
				 root_it != roots.rend() && hit.kind == Hit::Kind::None;
				 ++root_it) {
				Entity root{ *root_it };
				if (!root || !IsTilemap(root)) {
					continue;
				}

				Tilemap map{ root };
				const auto& tiles{ map.GetData().tiles };
				for (auto tile_it{ tiles.rbegin() }; tile_it != tiles.rend(); ++tile_it) {
					if (!Contains(TileSelectionBounds(map, *tile_it), world)) {
						continue;
					}

					hit.kind = Hit::Kind::Tile;
					hit.tilemap = map;
					hit.tile_cell = tile_it->coordinate;
					break;
				}
			}
		}

		if (hit.kind != Hit::Kind::None) {
			break;
		}

		// No authored object on this layer covered the cursor, so a generator on
		// this same layer may be selected before looking through to lower layers.
		for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
			Entity root{ *root_it };
			if (GeneratorContainsWorld(root, world)) {
				hit.kind = Hit::Kind::Generator;
				hit.entity = root;
				break;
			}
		}
	}

	switch (hit.kind) {
		case Hit::Kind::Generator: {
			const UUID uuid{ hit.entity.Get<UUID>() };
			if (toggle && selected_generator_ == uuid) {
				selected_generator_.reset();
				ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
				return;
			}

			ClearSelection();
			selected_generator_ = uuid;
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(hit.entity, false);
			return;
		}

		case Hit::Kind::Tile: {
			if (!additive && !toggle) {
				selected_tile_cells_.clear();
			}

			// Tile multi-selection is intentionally scoped to one Tilemap. Clicking
			// a tile in another Tilemap starts a fresh tile selection, while Shift/
			// Ctrl continue to work within the same Tilemap.
			const UUID map_uuid{ hit.tilemap.Get<UUID>() };
			if (!selected_tilemap_.has_value() || *selected_tilemap_ != map_uuid) {
				selected_tile_cells_.clear();
			}

			selected_entities_.clear();
			selected_generator_.reset();
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
			selected_tilemap_ = map_uuid;

			const auto it{
				std::ranges::find(selected_tile_cells_, hit.tile_cell)
			};
			if (toggle && it != selected_tile_cells_.end()) {
				selected_tile_cells_.erase(it);
			} else if (it == selected_tile_cells_.end()) {
				selected_tile_cells_.push_back(hit.tile_cell);
			}

			if (selected_tile_cells_.empty()) {
				selected_tilemap_.reset();
			}
			return;
		}

		case Hit::Kind::Entity: {
			if (!additive && !toggle) {
				selected_entities_.clear();
			}
			selected_generator_.reset();
			selected_tilemap_.reset();
			selected_tile_cells_.clear();

			const UUID uuid{ hit.entity.Get<UUID>() };
			const auto it{ std::ranges::find(selected_entities_, uuid) };
			if (toggle && it != selected_entities_.end()) {
				selected_entities_.erase(it);

				Entity primary{
					ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity()
				};
				if (primary && primary.Get<UUID>() == uuid) {
					if (!selected_entities_.empty()) {
						ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(
							scene.GetEntity(selected_entities_.back()),
							false
						);
					} else {
						ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
					}
				}
				return;
			}

			if (it == selected_entities_.end()) {
				selected_entities_.push_back(uuid);
			}
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(hit.entity, false);
			return;
		}

		case Hit::Kind::None:
		default:
			if (!additive && !toggle) {
				ClearSelection();
				ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
			}
			return;
	}
}

void PaintEditor::SelectMarquee(
	EditorContext& ctx, Scene& scene, V2_float a, V2_float b, bool additive, bool toggle
) {
	const PaintSelectionRect selection{ MakeSelectionRect(a, b) };

	// Marquee selection is scene-wide, just like click selection. The active paint layer
	// controls where new content is authored; it must not constrain what the selection tool
	// can reach. Tile selection still intentionally belongs to one Tilemap at a time, so when
	// several tile layers/maps overlap the marquee we use the visually topmost Tilemap that
	// has at least one intersecting authored tile.
	struct TileMarqueeHit {
		Tilemap map{};
		std::vector<V2_int> cells{};
		std::size_t layer_index{};
	};

	std::optional<TileMarqueeHit> tile_hit;
	std::optional<std::size_t> top_entity_hit_layer;
	const auto& layers{ scene.GetLayers().GetLayers() };

	for (std::size_t reverse_index{ layers.size() }; reverse_index > 0; --reverse_index) {
		const std::size_t layer_index{ reverse_index - 1 };
		const SceneLayer& layer{ layers[layer_index] };
		if (!layer.visible || layer.locked || !layer.selectable) {
			continue;
		}

		auto roots{ scene.GetLayers().GetRootEntities(scene, layer.id) };
		SortByLocalDepth(roots);

		if (layer.kind == SceneLayerKind::Entity) {
			bool overlaps_entity{};
			for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
				Entity root{ *root_it };
				if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
					IsProtectedSceneEntity(scene, root)) {
					continue;
				}

				const auto bounds{ EntitySelectionBounds(root) };
				if (bounds.has_value() && Overlaps(selection, *bounds)) {
					overlaps_entity = true;
					break;
				}
			}

			if (overlaps_entity && !top_entity_hit_layer.has_value()) {
				top_entity_hit_layer = layer_index;
			}
			continue;
		}

		if (layer.kind != SceneLayerKind::Tile || tile_hit.has_value()) {
			continue;
		}

		for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
			Entity root{ *root_it };
			if (!root || !IsTilemap(root)) {
				continue;
			}

			Tilemap map{ root };
			std::vector<V2_int> cells;
			for (const TilemapTile& tile : map.GetData().tiles) {
				if (Overlaps(selection, TileSelectionBounds(map, tile))) {
					cells.push_back(tile.coordinate);
				}
			}

			if (!cells.empty()) {
				tile_hit = TileMarqueeHit{
					.map = map,
					.cells = std::move(cells),
					.layer_index = layer_index,
				};
				break;
			}
		}
	}

	// If the highest authored content intersected by the marquee is a tile layer, select the
	// intersecting tiles from that Tilemap even when some completely different layer is active.
	// An intersecting entity on a visually higher layer blocks the tile layer, matching the
	// click-through rule used by SelectClick().
	const bool select_tiles{
		tile_hit.has_value() &&
		(!top_entity_hit_layer.has_value() || tile_hit->layer_index > *top_entity_hit_layer)
	};

	if (select_tiles) {
		const UUID map_uuid{ tile_hit->map.Get<UUID>() };

		// A tile selection cannot span two Tilemaps with the current selection representation.
		// Crossing onto another visible Tilemap therefore starts a fresh tile selection rather
		// than interpreting the old cells in the new map's coordinate space.
		if (!selected_tilemap_.has_value() || *selected_tilemap_ != map_uuid) {
			selected_tile_cells_.clear();
		} else if (!additive && !toggle) {
			selected_tile_cells_.clear();
		}

		selected_entities_.clear();
		selected_generator_.reset();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		selected_tilemap_ = map_uuid;

		for (V2_int cell : tile_hit->cells) {
			const auto it{ std::ranges::find(selected_tile_cells_, cell) };
			if (toggle && it != selected_tile_cells_.end()) {
				selected_tile_cells_.erase(it);
			} else if (it == selected_tile_cells_.end()) {
				selected_tile_cells_.push_back(cell);
			}
		}

		if (selected_tile_cells_.empty()) {
			selected_tilemap_.reset();
		}
		return;
	}

	// Entity marquee behavior remains scene-wide across all selectable entity layers. This is
	// intentionally unchanged from the selection rework; the only difference is that a visually
	// higher tile layer can now win the marquee in the same way it wins a click.
	if (!additive && !toggle) {
		selected_entities_.clear();
	}
	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();

	Entity primary{};
	for (const SceneLayer& layer : layers) {
		if (layer.kind != SceneLayerKind::Entity || !layer.visible || layer.locked ||
			!layer.selectable) {
			continue;
		}
		for (Entity root : scene.GetLayers().GetRootEntities(scene, layer.id)) {
			if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
				IsProtectedSceneEntity(scene, root)) {
				continue;
			}
			const auto bounds{ EntitySelectionBounds(root) };
			if (!bounds.has_value() || !Overlaps(selection, *bounds)) {
				continue;
			}

			const UUID uuid{ root.Get<UUID>() };
			const auto it{ std::ranges::find(selected_entities_, uuid) };
			if (toggle && it != selected_entities_.end()) {
				selected_entities_.erase(it);
			} else {
				if (it == selected_entities_.end()) {
					selected_entities_.push_back(uuid);
				}
				primary = root;
			}
		}
	}

	if (primary) {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(primary, false);
		return;
	}

	Entity current{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	if (!current || !std::ranges::contains(selected_entities_, current.Get<UUID>())) {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
	}
}
void PaintEditor::SelectBrush(EditorContext& ctx, Scene& scene, V2_float world, bool remove) {
	SceneLayer* active_layer{ ResolveActiveLayer(scene) };
	if (!active_layer || active_layer->locked || !active_layer->selectable) {
		return;
	}

	const V2_int center{ WorldToActiveCell(scene, world) };
	const auto brush_cells{ SelectionBrushCells(center) };
	const V2_float grid_size{ ActiveGridSize(scene) };
	auto brush_rect = [&](V2_int cell) {
		const V2_float min{ ActiveCellToWorld(scene, cell) };
		return PaintSelectionRect{ .min = min, .max = min + grid_size };
	};

	if (active_layer->kind == SceneLayerKind::Tile) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		selected_entities_.clear();
		selected_generator_.reset();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		selected_tilemap_ = map.Get<UUID>();

		for (const TilemapTile& tile : map.GetData().tiles) {
			const PaintSelectionRect tile_bounds{ TileSelectionBounds(map, tile) };
			const bool overlaps{ std::ranges::any_of(brush_cells, [&](V2_int cell) {
				return Overlaps(tile_bounds, brush_rect(cell));
			}) };
			if (!overlaps) {
				continue;
			}
			const auto it{ std::ranges::find(selected_tile_cells_, tile.coordinate) };
			if (remove) {
				if (it != selected_tile_cells_.end()) {
					selected_tile_cells_.erase(it);
				}
			} else if (it == selected_tile_cells_.end()) {
				selected_tile_cells_.push_back(tile.coordinate);
			}
		}
		if (selected_tile_cells_.empty()) {
			selected_tilemap_.reset();
		}
		return;
	}

	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();

	Entity primary{};
	for (const SceneLayer& layer : scene.GetLayers().GetLayers()) {
		if (layer.kind != SceneLayerKind::Entity || !layer.visible || layer.locked ||
			!layer.selectable) {
			continue;
		}
		for (Entity root : scene.GetLayers().GetRootEntities(scene, layer.id)) {
			if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
				IsProtectedSceneEntity(scene, root)) {
				continue;
			}
			const auto bounds{ EntitySelectionBounds(root) };
			if (!bounds.has_value()) {
				continue;
			}
			const bool overlaps{ std::ranges::any_of(brush_cells, [&](V2_int cell) {
				return Overlaps(*bounds, brush_rect(cell));
			}) };
			if (!overlaps) {
				continue;
			}

			const UUID uuid{ root.Get<UUID>() };
			const auto it{ std::ranges::find(selected_entities_, uuid) };
			if (remove) {
				if (it != selected_entities_.end()) {
					selected_entities_.erase(it);
				}
			} else {
				if (it == selected_entities_.end()) {
					selected_entities_.push_back(uuid);
				}
				primary = root;
			}
		}
	}

	if (primary) {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(primary, false);
		return;
	}

	Entity current{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	if (current && !std::ranges::contains(selected_entities_, current.Get<UUID>())) {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
	}
}

bool PaintEditor::SelectionHitAtWorld(Scene& scene, V2_float world) const {
	if (selected_generator_.has_value()) {
		Entity selected{ scene.GetEntity(*selected_generator_) };
		if (const auto bounds{ GeneratorSelectionBounds(selected) };
			bounds.has_value() && Contains(*bounds, world)) {
			return true;
		}
	}

	for (UUID uuid : selected_entities_) {
		Entity entity{ scene.GetEntity(uuid) };
		if (entity) {
			const auto bounds{ EntitySelectionBounds(entity) };
			if (bounds.has_value() && Contains(*bounds, world)) {
				return true;
			}
		}
	}

	if (selected_tilemap_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_tilemap_) };
		if (entity && IsTilemap(entity)) {
			Tilemap map{ entity };
			for (V2_int cell : selected_tile_cells_) {
				if (const TilemapTile* tile{ map.FindTile(cell) };
					tile && Contains(TileSelectionBounds(map, *tile), world)) {
					return true;
				}
			}
		}
	}
	return false;
}

void PaintEditor::SnapSelectionToGrid(EditorContext& ctx, Scene& scene) {
	if (!HasSelection()) {
		return;
	}

	std::vector<std::pair<UUID, Transform>> entity_before;
	std::vector<std::pair<UUID, Transform>> entity_after;
	std::optional<UUID> tilemap_uuid;
	std::optional<::ptgn::impl::TilemapData> tilemap_before;
	std::optional<::ptgn::impl::TilemapData> tilemap_after;
	const std::vector<V2_int> selected_cells_before{ selected_tile_cells_ };
	std::vector<V2_int> selected_cells_after{ selected_tile_cells_ };

	auto snap_axis = [](float value, float origin, float step) {
		step = std::max(1.0f, step);
		return origin + std::round((value - origin) / step) * step;
	};

	if (selected_generator_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_generator_) };
		if (entity && IsPaintGenerator(entity) && entity.Has<Transform>()) {
			const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
			const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
			if (layer && !layer->locked) {
				V2_float size{ entity_grid_size_ };
				V2_float origin{ entity_grid_offset_ };
				if (layer->kind == SceneLayerKind::Tile) {
					Tilemap grid_map{};
					const auto& generator_data{ PaintGenerator{ entity }.GetData() };
					if (generator_data.target_tilemap.has_value()) {
						Entity target{ scene.GetEntity(*generator_data.target_tilemap) };
						if (target && IsTilemap(target) &&
							scene.GetLayers().GetLayerId(target) == layer->id) {
							grid_map = Tilemap{ target };
						}
					}
					if (!grid_map) {
						for (Entity root : scene.GetLayers().GetRootEntities(scene, layer->id)) {
							if (IsTilemap(root)) {
								grid_map = Tilemap{ root };
								break;
							}
						}
					}
					if (grid_map) {
						size = grid_map.GetData().cell_size;
						origin = GetWorldPosition(grid_map);
					}
				}
				const Transform before{ GetWorldTransform(entity) };
				Transform after{ before };
				const V2_float generator_grid_origin{
					GetWorldPosition(entity) + PaintGenerator{ entity }.GetData().grid_offset
				};
				const V2_float snapped_grid_origin{
					snap_axis(generator_grid_origin.x, origin.x, size.x),
					snap_axis(generator_grid_origin.y, origin.y, size.y),
				};
				after.position += snapped_grid_origin - generator_grid_origin;
				if (before != after) {
					entity_before.emplace_back(entity.Get<UUID>(), before);
					entity_after.emplace_back(entity.Get<UUID>(), after);
					SetWorldTransform(entity, after);
				}
			}
		}
	} else if (!selected_entities_.empty()) {
		const V2_float size{ entity_grid_size_ };
		const V2_float origin{ entity_grid_offset_ };
		for (UUID uuid : selected_entities_) {
			Entity entity{ scene.GetEntity(uuid) };
			if (!entity || !entity.Has<Transform>() || IsProtectedSceneEntity(scene, entity)) {
				continue;
			}
			const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
			const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
			if (!layer || layer->locked) {
				continue;
			}

			const Transform before{ GetWorldTransform(entity) };
			Transform after{ before };
			const Origin draw_origin{ entity.GetOrDefault<Origin>() };
			const V2_float anchor_offset{ size * 0.5f - GetOffset(draw_origin, size) };
			after.position = {
				origin.x + std::round(
					(before.position.x - origin.x - anchor_offset.x) / std::max(1.0f, size.x)
				) * std::max(1.0f, size.x) + anchor_offset.x,
				origin.y + std::round(
					(before.position.y - origin.y - anchor_offset.y) / std::max(1.0f, size.y)
				) * std::max(1.0f, size.y) + anchor_offset.y,
			};
			if (before != after) {
				entity_before.emplace_back(uuid, before);
				entity_after.emplace_back(uuid, after);
				SetWorldTransform(entity, after);
			}
		}
	} else if (selected_tilemap_.has_value() && !selected_tile_cells_.empty()) {
		Entity entity{ scene.GetEntity(*selected_tilemap_) };
		if (entity && IsTilemap(entity)) {
			const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
			const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
			if (layer && !layer->locked) {
				Tilemap map{ entity };
				tilemap_uuid = map.Get<UUID>();
				tilemap_before = map.GetData();
				std::vector<TilemapTile> moved;
				for (V2_int cell : selected_tile_cells_) {
					if (const TilemapTile* tile{ map.FindTile(cell) }) {
						moved.push_back(*tile);
					}
				}
				for (V2_int cell : selected_tile_cells_) {
					map.EraseTile(cell);
				}
				selected_tile_cells_.clear();
				const V2_float cell_size{ map.GetData().cell_size };
				const V2_float map_origin{ GetWorldPosition(map) };
				for (TilemapTile tile : moved) {
					const V2_float anchor{ map.CellToWorld(tile.coordinate) + tile.offset };
					const V2_int target{
						static_cast<int>(std::lround(
							(anchor.x - map_origin.x) / std::max(1.0f, cell_size.x)
						)),
						static_cast<int>(std::lround(
							(anchor.y - map_origin.y) / std::max(1.0f, cell_size.y)
						)),
					};
					tile.coordinate = target;
					tile.offset = {};
					map.SetTile(std::move(tile));
					if (!std::ranges::contains(selected_tile_cells_, target)) {
						selected_tile_cells_.push_back(target);
					}
				}
				tilemap_after = map.GetData();
				selected_cells_after = selected_tile_cells_;
			}
		}
	}

	bool changed{ !entity_before.empty() };
	if (tilemap_before.has_value() && tilemap_after.has_value()) {
		json before_json = *tilemap_before;
		json after_json = *tilemap_after;
		changed = changed || before_json != after_json;
	}
	if (!changed) {
		return;
	}

	Scene* scene_ptr{ &scene };
	PaintEditor* paint{ this };
	ctx.undo.PushApplied(
		"Snap Selection to Grid",
		[scene_ptr, paint, entity_before, tilemap_uuid, tilemap_before, selected_cells_before]() {
			for (const auto& [uuid, transform] : entity_before) {
				if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
					SetWorldTransform(entity, transform);
				}
			}
			if (tilemap_uuid && tilemap_before) {
				if (Entity entity{ scene_ptr->GetEntity(*tilemap_uuid) };
					entity && entity.Has<::ptgn::impl::TilemapData>()) {
					entity.Get<::ptgn::impl::TilemapData>() = *tilemap_before;
					paint->selected_tile_cells_ = selected_cells_before;
				}
			}
		},
		[scene_ptr, paint, entity_after, tilemap_uuid, tilemap_after, selected_cells_after]() {
			for (const auto& [uuid, transform] : entity_after) {
				if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
					SetWorldTransform(entity, transform);
				}
			}
			if (tilemap_uuid && tilemap_after) {
				if (Entity entity{ scene_ptr->GetEntity(*tilemap_uuid) };
					entity && entity.Has<::ptgn::impl::TilemapData>()) {
					entity.Get<::ptgn::impl::TilemapData>() = *tilemap_after;
					paint->selected_tile_cells_ = selected_cells_after;
				}
			}
		}
	);
}

void PaintEditor::BeginMove(EditorContext&, Scene& scene, V2_float world) {
	if (!HasSelection()) {
		return;
	}

	move_ = {};
	move_.start_mouse_world = world;

	if (selected_generator_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_generator_) };
		if (!entity || !IsPaintGenerator(entity) || !entity.Has<Transform>()) {
			return;
		}
		const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
		const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
		if (!layer || layer->locked || !layer->selectable) {
			return;
		}
		move_.entity_before.emplace_back(entity.Get<UUID>(), GetWorldTransform(entity));
		move_.active = true;
		return;
	}

	if (!selected_entities_.empty()) {
		for (UUID uuid : selected_entities_) {
			Entity entity{ scene.GetEntity(uuid) };
			if (!entity || !entity.Has<Transform>() || IsProtectedSceneEntity(scene, entity) ||
				IsTilemap(entity) || IsPaintGenerator(entity)) {
				continue;
			}
			const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
			const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
			if (!layer || layer->locked || !layer->selectable) {
				continue;
			}
			move_.entity_before.emplace_back(uuid, GetWorldTransform(entity));
		}
		move_.active = !move_.entity_before.empty();
		return;
	}

	if (!selected_tilemap_.has_value() || selected_tile_cells_.empty()) {
		return;
	}
	Entity entity{ scene.GetEntity(*selected_tilemap_) };
	if (!entity || !IsTilemap(entity)) {
		return;
	}
	const auto layer_id{ scene.GetLayers().GetLayerId(entity) };
	const SceneLayer* layer{ layer_id ? scene.GetLayers().Find(*layer_id) : nullptr };
	if (!layer || layer->locked || !layer->selectable) {
		return;
	}
	Tilemap map{ entity };
	move_.tilemap = map.Get<UUID>();
	move_.tile_cells = selected_tile_cells_;
	move_.tilemap_before = map.GetData();
	move_.active = true;
}

void PaintEditor::UpdateMove(EditorContext&, Scene& scene, V2_float world) {
	if (!move_.active) {
		return;
	}

	PaintMoveSnapMode effective{ move_snap_ };
	if (CtrlDown()) {
		effective = effective == PaintMoveSnapMode::Grid ? PaintMoveSnapMode::Free
												 : PaintMoveSnapMode::Grid;
	}

	V2_float unit{ entity_grid_size_ };
	if (selected_generator_.has_value()) {
		if (Entity entity{ scene.GetEntity(*selected_generator_) }; entity && IsPaintGenerator(entity)) {
			unit = PaintGenerator{ entity }.GetData().grid_size;
		}
	} else if (move_.tilemap.has_value()) {
		if (Entity entity{ scene.GetEntity(*move_.tilemap) }; entity && IsTilemap(entity)) {
			unit = Tilemap{ entity }.GetData().cell_size;
		}
	}
	unit.x = std::max(1.0f, unit.x);
	unit.y = std::max(1.0f, unit.y);

	V2_float delta{ world - move_.start_mouse_world };
	if (effective == PaintMoveSnapMode::Grid) {
		delta.x = std::round(delta.x / unit.x) * unit.x;
		delta.y = std::round(delta.y / unit.y) * unit.y;
	}

	if (!move_.entity_before.empty()) {
		for (const auto& [uuid, before] : move_.entity_before) {
			if (Entity entity{ scene.GetEntity(uuid) }) {
				Transform next{ before };
				next.position += delta;
				SetWorldTransform(entity, next);
			}
		}
		return;
	}

	if (!move_.tilemap.has_value() || !move_.tilemap_before.has_value()) {
		return;
	}
	Entity entity{ scene.GetEntity(*move_.tilemap) };
	if (!entity || !entity.Has<::ptgn::impl::TilemapData>()) {
		return;
	}

	::ptgn::impl::TilemapData next{ *move_.tilemap_before };
	if (effective == PaintMoveSnapMode::Free) {
		for (auto& tile : next.tiles) {
			if (std::ranges::contains(move_.tile_cells, tile.coordinate)) {
				tile.offset += delta;
			}
		}
		selected_tile_cells_ = move_.tile_cells;
	} else {
		const V2_int cell_delta{
			static_cast<int>(std::lround(delta.x / unit.x)),
			static_cast<int>(std::lround(delta.y / unit.y)),
		};
		std::vector<TilemapTile> moved;
		for (const TilemapTile& tile : move_.tilemap_before->tiles) {
			if (std::ranges::contains(move_.tile_cells, tile.coordinate)) {
				moved.push_back(tile);
			}
		}
		std::erase_if(next.tiles, [&](const TilemapTile& tile) {
			return std::ranges::contains(move_.tile_cells, tile.coordinate);
		});
		selected_tile_cells_.clear();
		for (TilemapTile tile : moved) {
			const V2_int target{ tile.coordinate + cell_delta };
			std::erase_if(next.tiles, [&](const TilemapTile& existing) {
				return existing.coordinate == target;
			});
			tile.coordinate = target;
			next.tiles.push_back(std::move(tile));
			if (!std::ranges::contains(selected_tile_cells_, target)) {
				selected_tile_cells_.push_back(target);
			}
		}
	}
	entity.Get<::ptgn::impl::TilemapData>() = std::move(next);
}

void PaintEditor::CommitMove(EditorContext& ctx, Scene& scene) {
	if (!move_.active) {
		return;
	}

	if (!move_.entity_before.empty()) {
		std::vector<std::pair<UUID, Transform>> after;
		after.reserve(move_.entity_before.size());
		bool changed{};
		for (const auto& [uuid, before] : move_.entity_before) {
			if (Entity entity{ scene.GetEntity(uuid) }) {
				const Transform transform{ GetWorldTransform(entity) };
				after.emplace_back(uuid, transform);
				changed = changed || transform != before;
			}
		}
		if (changed) {
			Scene* scene_ptr{ &scene };
			const auto before{ move_.entity_before };
			ctx.undo.PushApplied(
				"Move Selection",
				[scene_ptr, before]() {
					for (const auto& [uuid, transform] : before) {
						if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
							SetWorldTransform(entity, transform);
						}
					}
				},
				[scene_ptr, after]() {
					for (const auto& [uuid, transform] : after) {
						if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
							SetWorldTransform(entity, transform);
						}
					}
				}
			);
		}
	}

	if (move_.tilemap.has_value() && move_.tilemap_before.has_value()) {
		Entity entity{ scene.GetEntity(*move_.tilemap) };
		if (entity && entity.Has<::ptgn::impl::TilemapData>()) {
			const ::ptgn::impl::TilemapData before{ *move_.tilemap_before };
			const ::ptgn::impl::TilemapData after{ entity.Get<::ptgn::impl::TilemapData>() };
			json a = before;
			json b = after;
			if (a != b) {
				Scene* scene_ptr{ &scene };
				PaintEditor* paint{ this };
				const UUID uuid{ *move_.tilemap };
				const auto before_cells{ move_.tile_cells };
				const auto after_cells{ selected_tile_cells_ };
				ctx.undo.PushApplied(
					"Move Selection",
					[scene_ptr, paint, uuid, before, before_cells]() {
						if (Entity e{ scene_ptr->GetEntity(uuid) };
							e && e.Has<::ptgn::impl::TilemapData>()) {
							e.Get<::ptgn::impl::TilemapData>() = before;
							paint->selected_tile_cells_ = before_cells;
						}
					},
					[scene_ptr, paint, uuid, after, after_cells]() {
						if (Entity e{ scene_ptr->GetEntity(uuid) };
							e && e.Has<::ptgn::impl::TilemapData>()) {
							e.Get<::ptgn::impl::TilemapData>() = after;
							paint->selected_tile_cells_ = after_cells;
						}
					}
				);
			}
		}
	}
	move_ = {};
}

void PaintEditor::EndMove(EditorContext& ctx, Scene& scene) {
	CommitMove(ctx, scene);
}

void PaintEditor::CancelMove(Scene& scene) {
	if (!move_.active) {
		return;
	}
	for (const auto& [uuid, transform] : move_.entity_before) {
		if (Entity entity{ scene.GetEntity(uuid) }) {
			SetWorldTransform(entity, transform);
		}
	}
	if (move_.tilemap.has_value() && move_.tilemap_before.has_value()) {
		if (Entity entity{ scene.GetEntity(*move_.tilemap) };
			entity && entity.Has<::ptgn::impl::TilemapData>()) {
			entity.Get<::ptgn::impl::TilemapData>() = *move_.tilemap_before;
		}
	}
	selected_tile_cells_ = move_.tile_cells;
	move_ = {};
}

Entity PaintEditor::FindGeneratorAtWorld(Scene& scene, V2_float world) const {
	const auto& layers{ scene.GetLayers().GetLayers() };
	for (auto layer_it{ layers.rbegin() }; layer_it != layers.rend(); ++layer_it) {
		const SceneLayer& layer{ *layer_it };
		if (!layer.visible || layer.locked || !layer.selectable) {
			continue;
		}

		auto roots{ scene.GetLayers().GetRootEntities(scene, layer.id) };
		SortByLocalDepth(roots);

		// Match the sandbox's scene hit order: a manually authored object on a higher
		// layer blocks generators on that layer and every layer beneath it.
		if (layer.kind == SceneLayerKind::Tile) {
			for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
				Entity root{ *root_it };
				if (!root || !IsTilemap(root)) {
					continue;
				}
				Tilemap map{ root };
				for (const TilemapTile& tile : map.GetData().tiles) {
					if (Contains(TileSelectionBounds(map, tile), world)) {
						return {};
					}
				}
			}
		} else if (layer.kind == SceneLayerKind::Entity) {
			for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
				Entity root{ *root_it };
				if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
					IsProtectedSceneEntity(scene, root)) {
					continue;
				}
				if (const auto bounds{ EntitySelectionBounds(root) };
					bounds.has_value() && Contains(*bounds, world)) {
					return {};
				}
			}
		}

		for (auto root_it{ roots.rbegin() }; root_it != roots.rend(); ++root_it) {
			Entity entity{ *root_it };
			if (GeneratorContainsWorld(entity, world)) {
				return entity;
			}
		}
	}
	return {};
}

void PaintEditor::HandleShortcuts(EditorContext&, Scene&) {
	const auto& io{ ImGui::GetIO() };
	if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || io.KeySuper) {
		return;
	}
	if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
		SetTool(PaintTool::Select);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_M, false)) {
		SetTool(PaintTool::Move);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
		SetTool(PaintTool::Pencil);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_B, false)) {
		SetTool(PaintTool::Brush);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_L, false)) {
		SetTool(PaintTool::Line);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_R, false)) {
		SetTool(PaintTool::Rectangle);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
		SetTool(PaintTool::Fill);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_E, false)) {
		SetTool(PaintTool::Erase);
	}
	if (ImGui::IsKeyPressed(ImGuiKey_K, false)) {
		SetTool(PaintTool::Eyedropper);
	}
}

bool PaintEditor::DrawViewportAndHandleInput(
	EditorContext& ctx, Scene& scene, Viewport image_viewport, Viewport presentation_viewport,
	const FrameContext& frame
) {
	EnsureLocalState(ctx);
	ValidateSceneState(scene);
	SyncHierarchySelection(ctx, scene);
	SceneLayer* layer{ ResolveActiveLayer(scene) };
	if (!layer) {
		return false;
	}

	if (active_brush_generator_.has_value()) {
		Entity active{ scene.GetEntity(*active_brush_generator_) };
		const bool still_compatible{ active && IsPaintGenerator(active) &&
									 tool_ == PaintTool::Brush &&
									 recipe_.commit_mode == PaintCommitMode::KeepGenerator &&
									 recipe_.operation == PaintBrushOperation::Paint &&
									 scene.GetLayers().GetLayerId(active) ==
										 std::optional<SceneLayerId>{ layer->id } };
		if (!still_compatible) {
			FinishActiveBrushGenerator(ctx, scene);
		}
	}

	const V2_float mouse_screen{ FromImGui(ImGui::GetIO().MousePos) };
	const bool inside{ Contains(image_viewport, mouse_screen) };
	const V2_float mouse_world{ ScreenToWorld(mouse_screen, frame, presentation_viewport) };
	if (stroke_.active && inside) {
		// Keep the pending generator preview on the current cursor this frame rather than
		// waiting for the input update below. UpdateStroke will commit the same point later.
		stroke_.current_world = mouse_world;
	}

	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	DrawTilemaps(ctx, scene, draw, presentation_viewport, frame);
	DrawGenerators(ctx, scene, draw, image_viewport, presentation_viewport, frame);
	DrawActiveGeneratorPreview(ctx, scene, draw, presentation_viewport, frame);
	DrawGrid(scene, draw, image_viewport, presentation_viewport, frame);
	DrawSelectionOverlay(scene, draw, presentation_viewport, frame);

	if (inside) {
		DrawToolPreview(scene, draw, mouse_world, presentation_viewport, frame);
	}
	const bool pointer_operation_active{ stroke_.active || move_.active || selection_drag_active_ };
	if ((!ctx.local.state.viewport.hovered && !pointer_operation_active) || ImGui::GetIO().WantTextInput) {
		return false;
	}

	if (active_brush_generator_.has_value() && ImGui::GetIO().KeyCtrl &&
		ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
		UndoActiveBrushStroke(ctx, scene);
		return true;
	}

	HandleShortcuts(ctx, scene);

	if (active_brush_generator_.has_value() && !stroke_.active &&
		ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
		FinishActiveBrushGenerator(ctx, scene);
		return true;
	}

	if (tool_ == PaintTool::Select && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		selection_drag_start_ = {};
		selection_drag_active_ = false;
		ClearSelection();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		return true;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		const bool had_active_operation{ stroke_.active || move_.active || selection_drag_active_ ||
										 active_brush_generator_.has_value() };
		if (active_brush_generator_.has_value() && !stroke_.active) {
			CancelActiveBrushGenerator(ctx, scene);
			return true;
		}
		if (stroke_.active) {
			CancelStroke(scene);
		}
		if (move_.active) {
			CancelMove(scene);
		}
		selection_drag_start_ = {};
		selection_drag_active_ = false;
		if (!had_active_operation && tool_ == PaintTool::Select) {
			ClearSelection();
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		}
		return true;
	}
	if (!inside && !stroke_.active && !move_.active && !selection_drag_active_) {
		return false;
	}

	if (tool_ == PaintTool::Move) {
		if (move_.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			UpdateMove(ctx, scene, mouse_world);
			return true;
		}
		if (move_.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			EndMove(ctx, scene);
			return true;
		}
		if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			const bool additive{ ImGui::GetIO().KeyShift };
			const bool toggle{ CtrlDown() };
			if (!SelectionHitAtWorld(scene, mouse_world)) {
				SelectClick(ctx, scene, mouse_world, additive, toggle);
			}
			if (HasSelection() && SelectionHitAtWorld(scene, mouse_world)) {
				BeginMove(ctx, scene, mouse_world);
			}
			return true;
		}
		return move_.active;
	}

	if (tool_ == PaintTool::Select) {
		if (select_mode_ == PaintSelectMode::Brush) {
			selection_drag_start_ = {};
			selection_drag_active_ = false;
			if (inside && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				SelectBrush(ctx, scene, mouse_world, CtrlDown());
				return true;
			}
			return false;
		}

		if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			selection_drag_start_ = mouse_world;
			selection_drag_active_ = true;
			return true;
		}

		if (selection_drag_active_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			V2_float selection_end{ mouse_world };
			const bool additive{ ImGui::GetIO().KeyShift };
			const bool toggle{ CtrlDown() };

			const V2_float start_screen{
				WorldToScreen(selection_drag_start_, frame, presentation_viewport)
			};
			const V2_float end_screen{ WorldToScreen(mouse_world, frame, presentation_viewport) };
			const V2_float screen_delta{ end_screen - start_screen };
			const float screen_distance{
				std::sqrt(screen_delta.x * screen_delta.x + screen_delta.y * screen_delta.y)
			};

			if (screen_distance < 4.0f) {
				SelectClick(ctx, scene, mouse_world, additive, toggle);
			} else {
				if (additive) {
					selection_end = ConstrainSquareDrag(selection_drag_start_, mouse_world);
				}
				SelectMarquee(
					ctx, scene, selection_drag_start_, selection_end, additive, toggle
				);
			}

			selection_drag_start_ = {};
			selection_drag_active_ = false;
			return true;
		}
		return selection_drag_active_;
	}

	if (layer->locked) {
		return true;
	}

	if (tool_ == PaintTool::Eyedropper) {
		if (inside && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			EyedropAt(ctx, scene, mouse_world);
			return true;
		}
		return false;
	}

	if (tool_ == PaintTool::Fill) {
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inside) {
			BeginStroke(scene, mouse_world);
			FillAt(ctx, scene, mouse_world);
			EndStroke(ctx, scene, mouse_world);
			return true;
		}
		return false;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inside) {
		BeginStroke(scene, mouse_world);
		if (tool_ == PaintTool::Pencil || tool_ == PaintTool::Brush) {
			UpdateStroke(ctx, scene, mouse_world);
		}
		if (tool_ == PaintTool::Erase) {
			EraseAt(ctx, scene, mouse_world);
		}
		return true;
	}
	if (stroke_.active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		UpdateStroke(ctx, scene, mouse_world);
		return true;
	}
	if (stroke_.active && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		EndStroke(ctx, scene, mouse_world);
		return true;
	}
	return false;
}

} // namespace ptgn::editor
