#include "editor/paint/paint_editor.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
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
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "core/assert.h"
#include "core/util/file.h"
#include "core/math/angle.h"
#include "core/math/geometry/rect.h"
#include "core/util/string.h"
#include "editor/color_picker.h"
#include "editor/editor.h"
#include "editor/editor_context.h"
#include "editor/editor_icons.h"
#include "editor/modal_scale.h"
#include "editor/renamable_item.h"
#include "panels/scene_hierarchy.h"
#include "panels/scene_list.h"
#include "panels/inspector_layout.h"
#include "platform/file_dialog.h"
#include "platform/window.h"
#include "renderer/renderer.h"
#include "runtime/animation/animation.h"
#include "runtime/asset/asset_manager.h"
#include "runtime/asset/prefab.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/ecs/entity_serialization.h"
#include "runtime/ecs/group_stack.h"
#include "runtime/ecs/tag.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/frame_context.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/shape.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/visible.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_context.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/slider.h"
#include "runtime/world/autotile.h"
#include "runtime/world/chunk.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"
#include "serialization/json/json.h"

namespace ptgn::editor {

namespace {

constexpr ImU32 kSelection{ IM_COL32(78, 190, 255, 235) };
constexpr ImU32 kPreview{ IM_COL32(255, 208, 70, 220) };
constexpr std::size_t kMaxFloodCells{ 50000 };
constexpr const char* kTileLibraryEntryDragDropPayload{ "PTGN_TILE_LIBRARY_ENTRY" };

struct GroupSnapshot {
	std::vector<std::string> groups;
	std::unordered_map<std::string, std::string> members;
	std::vector<std::string> autotile_groups{};

	bool operator==(const GroupSnapshot&) const = default;
};

template <typename Entries>
[[nodiscard]] GroupSnapshot CaptureTileGroups(
	const std::vector<std::string>& groups, const Entries& entries
) {
	GroupSnapshot result{ .groups = groups, .members = {} };
	for (const auto& entry : entries) {
		result.members.insert_or_assign(entry.id, entry.group);
	}
	return result;
}

template <typename Apply>
void RecordGroupChange(
	EditorContext& ctx, std::string label, GroupSnapshot before, GroupSnapshot after, Apply apply
) {
	if (before == after) {
		return;
	}

	ctx.undo.PushApplied(
		std::move(label),
		[context = &ctx, state = std::move(before), apply]() { apply(*context, state); },
		[context = &ctx, state = std::move(after), apply]() { apply(*context, state); }
	);
}

[[nodiscard]] const PaintEntityExclusionMaskState* FindEntityExclusionMask(
	const PaintProjectState& project, std::string_view scene_key
) {
	for (const auto& mask : project.entity_exclusion_masks) {
		if (mask.scene_key == scene_key) {
			return &mask;
		}
	}
	return nullptr;
}

[[nodiscard]] bool IsEntityCellExcluded(
	const PaintProjectState& project, std::string_view scene_key, V2_int cell
) {
	const auto* mask{ FindEntityExclusionMask(project, scene_key) };
	return mask && std::ranges::binary_search(mask->cells, cell, [](V2_int a, V2_int b) {
			   return a.x == b.x ? a.y < b.y : a.x < b.x;
		   });
}

[[nodiscard]] std::vector<V2_int> GetEntityExclusionCells(
	const PaintProjectState& project, std::string_view scene_key
) {
	const auto* mask{ FindEntityExclusionMask(project, scene_key) };
	return mask ? mask->cells : std::vector<V2_int>{};
}

void SetEntityExclusionCells(
	PaintProjectState& project, std::string_view scene_key, std::vector<V2_int> cells
) {
	std::ranges::sort(cells, [](V2_int a, V2_int b) { return a.x == b.x ? a.y < b.y : a.x < b.x; });
	cells.erase(std::unique(cells.begin(), cells.end()), cells.end());

	auto& masks{ project.entity_exclusion_masks };
	for (auto it{ masks.begin() }; it != masks.end(); ++it) {
		if (it->scene_key == scene_key) {
			if (cells.empty()) {
				masks.erase(it);
			} else {
				it->cells = std::move(cells);
			}
			return;
		}
	}
	if (!cells.empty()) {
		masks.emplace_back(
			PaintEntityExclusionMaskState{
				.scene_key = std::string{ scene_key },
				.cells	   = std::move(cells),
			}
		);
	}
}

template <typename Undo, typename Redo>
void PushPaintUndo(
	EditorContext& ctx, const Scene& scene, std::string label, Undo&& undo, Redo&& redo
) {
	bool runtime{ scene.IsRuntime() };
	ctx.undo.PushApplied(
		std::move(label), std::forward<Undo>(undo), std::forward<Redo>(redo), !runtime, runtime,
		runtime
	);
}

Entity RecordPaintCreatedEntity(
	EditorContext& ctx, Entity entity, EditorSelection before_selection
) {
	bool runtime{ entity.GetScene().IsRuntime() };
	return ctx.commands.RecordCreatedEntity(
		entity, std::move(before_selection), !runtime, runtime, runtime
	);
}

void DeletePaintEntity(EditorContext& ctx, Entity entity) {
	bool runtime{ entity.GetScene().IsRuntime() };
	ctx.commands.DeleteEntity(entity, !runtime, runtime, runtime);
}

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
	V2_float presentation{ screen - presentation_viewport.GetCenter() };
	return ConvertPoint(presentation, Frame::Presentation, Frame::World, frame);
}

[[nodiscard]] V2_float WorldToScreen(
	V2_float world, const FrameContext& frame, Viewport presentation_viewport
) {
	return ConvertPoint(world, Frame::World, Frame::Presentation, frame) +
		   presentation_viewport.GetCenter();
}

[[nodiscard]] std::array<V2_float, 4> ViewportWorldCorners(
	Viewport image_viewport, Viewport presentation_viewport, const FrameContext& frame
) {
	V2_float minimum{ image_viewport.position };
	V2_float maximum{ image_viewport.position + image_viewport.size };
	return {
		ScreenToWorld(minimum, frame, presentation_viewport),
		ScreenToWorld({ maximum.x, minimum.y }, frame, presentation_viewport),
		ScreenToWorld(maximum, frame, presentation_viewport),
		ScreenToWorld({ minimum.x, maximum.y }, frame, presentation_viewport),
	};
}

[[nodiscard]] bool CellVisibleInViewport(
	V2_float world_min, V2_float cell_size, Viewport image_viewport, Viewport presentation_viewport,
	const FrameContext& frame
) {
	V2_float world_max{ world_min + cell_size };
	std::array<V2_float, 4> vertices{
		WorldToScreen(world_min, frame, presentation_viewport),
		WorldToScreen({ world_max.x, world_min.y }, frame, presentation_viewport),
		WorldToScreen(world_max, frame, presentation_viewport),
		WorldToScreen({ world_min.x, world_max.y }, frame, presentation_viewport),
	};
	V2_float minimum{ vertices.front() };
	V2_float maximum{ minimum };
	for (V2_float vertex : vertices) {
		minimum = Min(minimum, vertex);
		maximum = Max(maximum, vertex);
	}
	V2_float view_min{ image_viewport.position };
	V2_float view_max{ image_viewport.position + image_viewport.size };
	return minimum.x < view_max.x && maximum.x > view_min.x && minimum.y < view_max.y &&
		   maximum.y > view_min.y;
}

[[nodiscard]] bool IsClassic15GeneratorBrush(const PaintGeneratorRecipe& recipe) {
	return recipe.source_kind == PaintGeneratorSourceKind::Autotile &&
		   recipe.autotile_format == PaintGeneratorAutotileFormat::Classic15;
}

template <typename Add>
void AddGeneratorBrushCell(const PaintGeneratorRecipe& recipe, V2_int cell, Add add) {
	add(cell);
	if (IsClassic15GeneratorBrush(recipe)) {
		add(cell + V2_int{ 1, 0 });
		add(cell + V2_int{ 0, 1 });
		add(cell + V2_int{ 1, 1 });
	}
}

struct PrefabGeneratorPreview {
	TextureKey texture{};
	::ptgn::impl::TextureId texture_id{};
	V2_int texture_size{};
	std::vector<Transform> transform_path{};
	Origin origin{ Origin::Center };
	std::optional<::ptgn::impl::AnimationData> animation{};
};

[[nodiscard]] bool FindPrefabGeneratorPreview(
	Entity entity, std::vector<Transform>& transform_path, PrefabGeneratorPreview& result
) {
	if (!entity) {
		return false;
	}

	transform_path.push_back(entity.Has<Transform>() ? entity.Get<Transform>() : Transform{});

	if (auto texture{ entity.TryGet<TextureKey>() }; texture && *texture) {
		result.texture		  = *texture;
		result.transform_path = transform_path;
		result.origin		  = entity.Has<Origin>() ? entity.Get<Origin>() : Origin::Center;
		if (auto animation{ entity.TryGet<::ptgn::impl::AnimationData>() }) {
			result.animation = *animation;
		}
		transform_path.pop_back();
		return true;
	}

	if (HasChildren(entity)) {
		for (Entity child : GetChildren(entity)) {
			if (FindPrefabGeneratorPreview(child, transform_path, result)) {
				transform_path.pop_back();
				return true;
			}
		}
	}

	transform_path.pop_back();
	return false;
}

[[nodiscard]] std::optional<PrefabGeneratorPreview> ResolvePrefabGeneratorPreview(
	EditorContext& ctx, const PrefabKey& key
) {
	auto& assets{ ctx.editor.GetAssetManager() };
	Entity prefab_root{ assets.GetPrefabEntity(key) };
	if (!prefab_root) {
		return std::nullopt;
	}

	PrefabGeneratorPreview result;
	std::vector<Transform> transform_path;
	if (!FindPrefabGeneratorPreview(prefab_root, transform_path, result) || !result.texture) {
		return std::nullopt;
	}

	::ptgn::impl::AssetAccessor accessor{ assets };

	// The live prefab entity stores the same TextureKey as a scene instance. Resolve that
	// dependency through the catalog so preview residency follows ordinary asset rules.
	if (!accessor.Has<Texture>(result.texture)) {
		if (auto catalog{ assets.GetCatalogAsset(result.texture, AssetKind::Texture) };
			catalog.has_value()) {
			assets.Load(result.texture, catalog->source_path);
		}
	}

	auto records{ accessor.GetAssets() };
	auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
		return candidate.kind == AssetKind::Texture &&
			   candidate.key == static_cast<const AssetKey&>(result.texture);
	}) };
	if (record == records.end() || !record->preview.has_value() ||
		!record->preview->size.IsPositive()) {
		return std::nullopt;
	}

	result.texture_id	= record->preview->texture;
	result.texture_size = record->preview->size;
	return result;
}

[[nodiscard]] bool DrawPrefabGeneratorPreview(
	ImDrawList* draw, const PrefabGeneratorPreview& preview, V2_float placement,
	std::optional<float> rotation_degrees, std::optional<float> uniform_scale,
	const FrameContext& frame, Viewport presentation_viewport
) {
	if (!draw || !preview.texture_id || !preview.texture_size.IsPositive() ||
		preview.transform_path.empty()) {
		return false;
	}

	Transform world{ preview.transform_path.front() };
	world.position = placement;
	if (rotation_degrees.has_value()) {
		world.rotation = Degrees{ *rotation_degrees }.ToRad();
	}
	if (uniform_scale.has_value()) {
		world.scale = V2_float{ *uniform_scale };
	}

	for (std::size_t i{ 1 }; i < preview.transform_path.size(); ++i) {
		world = preview.transform_path[i].RelativeTo(world);
	}

	V2_int source_position{};
	V2_int source_size{ preview.texture_size };
	if (preview.animation.has_value()) {
		std::optional<V2_int> texture_size{ preview.texture_size };
		V2_int frame_size{ preview.animation->GetFrameSize(texture_size) };
		if (frame_size.IsPositive()) {
			source_size = frame_size;
			if constexpr (
				requires(const ::ptgn::impl::AnimationData& animation, std::optional<V2_int> size) {
					animation.GetCurrentFramePosition(size);
				}
			) {
				source_position = preview.animation->GetCurrentFramePosition(texture_size);
			} else if constexpr (requires(const ::ptgn::impl::AnimationData& animation) {
									 animation.GetCurrentFramePosition();
								 }) {
				source_position = preview.animation->GetCurrentFramePosition();
			}
		}
	}
	if (!source_size.IsPositive()) {
		return false;
	}

	auto texture_width{ static_cast<float>(preview.texture_size.x) };
	auto texture_height{ static_cast<float>(preview.texture_size.y) };
	auto u0{ static_cast<float>(source_position.x) / texture_width };
	auto v0{ static_cast<float>(source_position.y) / texture_height };
	auto u1{ static_cast<float>(source_position.x + source_size.x) / texture_width };
	auto v1{ static_cast<float>(source_position.y + source_size.y) / texture_height };

	auto vertices{ Rect{ V2_float{ source_size } }.GetWorldVertices(world, preview.origin) };

	draw->AddImageQuad(
		static_cast<ImTextureID>(preview.texture_id),
		ToImGui(WorldToScreen(vertices[0], frame, presentation_viewport)),
		ToImGui(WorldToScreen(vertices[1], frame, presentation_viewport)),
		ToImGui(WorldToScreen(vertices[2], frame, presentation_viewport)),
		ToImGui(WorldToScreen(vertices[3], frame, presentation_viewport)), ImVec2{ u0, v0 },
		ImVec2{ u1, v0 }, ImVec2{ u1, v1 }, ImVec2{ u0, v1 }, IM_COL32(255, 255, 255, 210)
	);
	return true;
}

[[nodiscard]] bool DrawPrefabGeneratorPreview(
	EditorContext& ctx,
	std::unordered_map<std::string, std::optional<PrefabGeneratorPreview>>& cache, ImDrawList* draw,
	const PrefabKey& key, V2_float placement, std::optional<float> rotation_degrees,
	std::optional<float> uniform_scale, const FrameContext& frame, Viewport presentation_viewport
) {
	auto [it, inserted]{ cache.try_emplace(key.value) };
	if (inserted) {
		it->second = ResolvePrefabGeneratorPreview(ctx, key);
	}
	if (!it->second.has_value()) {
		return false;
	}

	return DrawPrefabGeneratorPreview(
		draw, *it->second, placement, rotation_degrees, uniform_scale, frame, presentation_viewport
	);
}

struct PaintSelectionRect {
	V2_float min{};
	V2_float max{};
};

[[nodiscard]] bool Contains(const PaintSelectionRect& rect, V2_float point) {
	return point.x >= rect.min.x && point.x <= rect.max.x && point.y >= rect.min.y &&
		   point.y <= rect.max.y;
}

[[nodiscard]] bool Overlaps(const PaintSelectionRect& a, const PaintSelectionRect& b) {
	return a.min.x <= b.max.x && a.max.x >= b.min.x && a.min.y <= b.max.y && a.max.y >= b.min.y;
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
	Origin origin{ entity.GetOrDefault<Origin>() };

	if (entity.Has<Rect>()) {
		return BoundsFromVertices(entity.Get<Rect>().GetWorldVertices(transform, origin));
	}

	if (entity.Has<Circle>()) {
		return BoundsFromVertices(
			Rect{ entity.Get<Circle>().GetSize() }.GetWorldVertices(transform, origin)
		);
	}

	if (auto display_size{ GetDisplaySize(entity) };
		display_size.has_value() && display_size->IsPositive()) {
		// Sprite drawing normalizes absolute transform scale because GetDisplaySize() already
		// incorporates it. Mirror that here so hit testing matches the rendered rectangle.
		transform.scale.x = transform.scale.x < 0.0f ? -1.0f : 1.0f;
		transform.scale.y = transform.scale.y < 0.0f ? -1.0f : 1.0f;
		return BoundsFromVertices(Rect{ *display_size }.GetWorldVertices(transform, origin));
	}

	if (entity.Has<::ptgn::impl::TextData>()) {
		V2_float size{ Text{ entity }.GetSize() };
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
			if (auto child_bounds{ EntitySelectionBounds(child, depth + 1) }) {
				MergeBounds(result, *child_bounds);
			}
		}
	}
	return result;
}

// Pick UI content using the rendered entity ID, not the bounding box of its root.
// A Group Stack is a scene hierarchy root whose bounds span all of its descendants, so
// treating that root as a selectable entity hides the actual UI controls beneath it.
[[nodiscard]] Entity PickUIContent(
	EditorContext& ctx, Scene& scene, V2_float screen_position, Viewport image_viewport
) {
	RenderTarget target{ scene.GetRenderTarget() };

	if (!target || !target.Has<::ptgn::impl::FramebufferObject>() ||
		!image_viewport.size.IsPositive()) {
		return {};
	}

	V2_float local{ screen_position - image_viewport.position };

	if (local.x < 0.0f || local.y < 0.0f || local.x >= image_viewport.size.x ||
		local.y >= image_viewport.size.y) {
		return {};
	}

	::ptgn::impl::RendererAccessor renderer{ ctx.editor.GetRenderer() };
	auto framebuffer{
		static_cast<::ptgn::impl::FramebufferId>(target.Get<::ptgn::impl::FramebufferObject>())
	};

	if (!renderer.IsEntityPickingEnabled(framebuffer)) {
		return {};
	}

	V2_int size{ target.GetSize() };

	if (!size.IsPositive()) {
		return {};
	}

	V2_int pixel{
		static_cast<int>(local.x / image_viewport.size.x * static_cast<float>(size.x)),
		static_cast<int>(local.y / image_viewport.size.y * static_cast<float>(size.y)),
	};
	pixel = Clamp(pixel, V2_int{ 0, 0 }, size - V2_int{ 1, 1 });

	renderer.FlushBatch();
	auto picked_id{ renderer.ReadEntityId(framebuffer, pixel) };

	if (!picked_id.has_value() || picked_id.value() == ::ptgn::impl::kNoEntityId ||
		picked_id.value() < 0) {
		return {};
	}

	Entity picked{ scene.GetEntity(UUID{ picked_id.value() }) };

	if (!picked || !GetGroupStack(picked)) {
		return {};
	}

	// Managed sprite/text/shape parts should select the button or slider that
	// owns them, not an implementation detail inside the control.
	while (!ctx.editor.GetSettings().show_managed_ui_parts && picked && HasParent(picked) &&
		   picked.HasAny<
			   ButtonBackgroundVisuals, ButtonBorderVisuals, ButtonSpriteVisuals, ButtonTextVisuals,
			   ::ptgn::impl::SliderThumbData, ::ptgn::impl::SliderTrackData,
			   ::ptgn::impl::SliderTrackBackgroundData, ::ptgn::impl::SliderTrackBorderData,
			   ::ptgn::impl::SliderTrackSpriteData, ::ptgn::impl::SliderValueTextData>()) {
		picked = GetParent(picked);
	}

	if (!picked || picked.HasAny<GroupStackData, Group>() || !IsVisible(picked) || false) {
		return {};
	}

	return picked;
}

[[nodiscard]] PaintSelectionRect TileSelectionBounds(Tilemap map, const TilemapTile& tile) {
	const auto& data{ map.GetData() };
	V2_float visual_size{ tile.pixel_size.IsPositive() ? V2_float{ tile.pixel_size }
													   : data.cell_size };
	V2_float anchor{ map.CellToWorld(tile.coordinate) + tile.offset };
	V2_float center{ anchor + GetOffset(tile.origin, visual_size) };
	return {
		.min = center - visual_size * 0.5f,
		.max = center + visual_size * 0.5f,
	};
}

[[nodiscard]] V2_float ConstrainSquareDrag(V2_float start, V2_float current) {
	float dx{ current.x - start.x };
	float dy{ current.y - start.y };
	float side{ std::max(std::abs(dx), std::abs(dy)) };
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

	V2_float entity_offset{ GetWorldPosition(generator) };
	V2_float grid_origin{ data.grid_offset + entity_offset };
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
		last->x	 = std::max(last->x, cell.x);
		last->y	 = std::max(last->y, cell.y);
	};

	if (data.geometry == PaintGeneratorGeometry::Rectangle) {
		V2_int a{ to_cell(data.start + entity_offset) };
		V2_int b{ to_cell(data.end + entity_offset) };
		include({ std::min(a.x, b.x), std::min(a.y, b.y) });
		include({ std::max(a.x, b.x), std::max(a.y, b.y) });
	} else if (data.geometry == PaintGeneratorGeometry::Line) {
		V2_int a{ to_cell(data.start + entity_offset) };
		V2_int b{ to_cell(data.end + entity_offset) };
		int dx{ std::abs(b.x - a.x) };
		int dy{ -std::abs(b.y - a.y) };
		int sx{ a.x < b.x ? 1 : -1 };
		int sy{ a.y < b.y ? 1 : -1 };
		int half{ std::max(1, data.line_thickness) / 2 };
		int error{ dx + dy };
		for (;;) {
			include(a - V2_int{ half, half });
			include(a + V2_int{ half, half });
			if (a == b) {
				break;
			}
			int e2{ 2 * error };
			if (e2 >= dy) {
				error += dy;
				a.x	  += sx;
			}
			if (e2 <= dx) {
				error += dx;
				a.y	  += sy;
			}
		}
	} else {
		for (const auto& point : data.stroke_points) {
			V2_int center{ to_cell(point.position + entity_offset) };
			int diameter{ std::max(1, point.diameter) };
			int low{ -(diameter / 2) };
			int high{ low + diameter - 1 };
			int extra{ IsClassic15GeneratorBrush(data.recipe) ? 1 : 0 };
			include(center + V2_int{ low, low });
			include(center + V2_int{ high + extra, high + extra });
		}
	}

	if (!first.has_value() || !last.has_value()) {
		return std::nullopt;
	}
	return PaintSelectionRect{
		.min =
			grid_origin +
			V2_float{ static_cast<float>(first->x), static_cast<float>(first->y) } * data.grid_size,
		.max = grid_origin +
			   V2_float{ static_cast<float>(last->x + 1), static_cast<float>(last->y + 1) } *
				   data.grid_size,
	};
}

[[nodiscard]] const char* ToolName(PaintTool tool) {
	switch (tool) {
		case PaintTool::Select:		return "Select";
		case PaintTool::Move:		return "Move";
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
	int result{ value % divisor };
	return result < 0 ? result + divisor : result;
}

[[nodiscard]] float BrushCircleCellOffset(int diameter) {
	return (std::max(1, diameter) % 2) == 0 ? 0.5f : 0.0f;
}

[[nodiscard]] V2_int ResolveStableBrushCenter(
	V2_int hovered, int diameter, std::optional<V2_int>& anchor
) {
	diameter = std::max(1, diameter);
	if ((diameter % 2) != 0) {
		anchor = hovered;
		return hovered;
	}

	if (!anchor.has_value()) {
		anchor = hovered;
		return hovered;
	}

	auto resolve_axis = [](int hovered_axis, int anchor_axis) {
		// Even brushes are centered on the boundary shared by four cells. Keep the brush fixed
		// while the pointer remains in either center cell on this axis, then shift only after
		// the pointer leaves that two cell center band.
		if (hovered_axis < anchor_axis - 1) {
			return hovered_axis + 1;
		}
		if (hovered_axis > anchor_axis) {
			return hovered_axis;
		}
		return anchor_axis;
	};

	anchor = V2_int{
		resolve_axis(hovered.x, anchor->x),
		resolve_axis(hovered.y, anchor->y),
	};
	return *anchor;
}

[[nodiscard]] V2_int GeneratorCellAtWorld(Entity entity, V2_float world) {
	PTGN_ASSERT(entity && IsPaintGenerator(entity));
	PaintGenerator generator{ entity };
	const auto& data{ generator.GetData() };
	V2_float origin{ data.grid_offset + GetWorldPosition(entity) };
	return {
		static_cast<int>(std::floor((world.x - origin.x) / std::max(1.0f, data.grid_size.x))),
		static_cast<int>(std::floor((world.y - origin.y) / std::max(1.0f, data.grid_size.y))),
	};
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

	V2_float entity_offset{ GetWorldPosition(generator) };
	V2_float origin{ data.grid_offset + entity_offset };
	auto to_cell = [&](V2_float point) {
		return V2_int{
			static_cast<int>(std::floor((point.x - origin.x) / data.grid_size.x)),
			static_cast<int>(std::floor((point.y - origin.y) / data.grid_size.y)),
		};
	};

	V2_int target{ to_cell(world) };
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
		V2_int a{ to_cell(data.start + entity_offset) };
		V2_int b{ to_cell(data.end + entity_offset) };
		int min_x{ std::min(a.x, b.x) };
		int max_x{ std::max(a.x, b.x) };
		int min_y{ std::min(a.y, b.y) };
		int max_y{ std::max(a.y, b.y) };
		if (target.x < min_x || target.x > max_x || target.y < min_y || target.y > max_y) {
			return false;
		}
		int l{ target.x - min_x };
		int r{ max_x - target.x };
		int t{ target.y - min_y };
		int bottom{ max_y - target.y };
		switch (data.area_mode) {
			case PaintGeneratorAreaMode::Fill: return true;
			case PaintGeneratorAreaMode::RandomFill:
				return Hash01(target) <= data.random_fill_density;
			case PaintGeneratorAreaMode::Outline:
				return l < data.area_thickness || r < data.area_thickness ||
					   t < data.area_thickness || bottom < data.area_thickness;
			case PaintGeneratorAreaMode::Corners: {
				int q{ std::max(1, data.area_thickness) };
				return (l < q && t < q) || (r < q && t < q) || (l < q && bottom < q) ||
					   (r < q && bottom < q);
			}
		}
	}

	if (data.geometry == PaintGeneratorGeometry::Line) {
		V2_int a{ to_cell(data.start + entity_offset) };
		V2_int b{ to_cell(data.end + entity_offset) };
		int dx{ std::abs(b.x - a.x) };
		int dy{ -std::abs(b.y - a.y) };
		int sx{ a.x < b.x ? 1 : -1 };
		int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};
		for (;;) {
			if (step % std::max(1, data.line_spacing) == 0) {
				int half{ std::max(1, data.line_thickness) / 2 };
				if (target.x >= a.x - half && target.x <= a.x + half && target.y >= a.y - half &&
					target.y <= a.y + half) {
					return true;
				}
			}
			if (a == b) {
				break;
			}
			int e2{ 2 * error };
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
		return false;
	}

	for (const auto& point : data.stroke_points) {
		V2_int center{ to_cell(point.position + entity_offset) };
		int diameter{ std::max(1, point.diameter) };
		int low{ -(diameter / 2) };
		int high{ low + diameter - 1 };
		auto contains = [&](V2_int candidate) {
			V2_int delta{ candidate - center };
			if (delta.x < low || delta.x > high || delta.y < low || delta.y > high) {
				return false;
			}
			if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
				float offset{ BrushCircleCellOffset(diameter) };
				float dx{ static_cast<float>(delta.x) + offset };
				float dy{ static_cast<float>(delta.y) + offset };
				return std::sqrt(dx * dx + dy * dy) <= static_cast<float>(diameter) * 0.5f;
			}
			return true;
		};
		if (contains(target)) {
			return true;
		}
		if (IsClassic15GeneratorBrush(data.recipe) &&
			(contains(target - V2_int{ 1, 0 }) || contains(target - V2_int{ 0, 1 }) ||
			 contains(target - V2_int{ 1, 1 }))) {
			return true;
		}
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

[[nodiscard]] std::vector<Entity> SceneRootEntities(Scene& scene) {
	std::vector<Entity> roots;
	for (Entity entity : scene.Entities()) {
		if (entity && !HasParent(entity)) {
			roots.emplace_back(entity);
		}
	}
	SortByLocalDepth(roots);
	return roots;
}

[[nodiscard]] bool IsProtectedSceneEntity(Scene& scene, Entity entity) {
	return entity == scene.GetRenderTarget() || entity == scene.GetCamera() ||
		   entity == scene.GetFixedCamera();
}

Entity RestoreEntityTree(Scene& scene, const SerializedEntity& input, Entity parent = {}) {
	Entity entity{ input.uuid.has_value() ? scene.CreateEntity(Tag{ input.tag }, *input.uuid)
										  : scene.CreateEntity(Tag{ input.tag }) };
	DeserializeEntity(input, entity);
	if (parent) {
		SetParent(entity, parent);
	}
	for (const auto& child : input.children) {
		RestoreEntityTree(scene, child, entity);
	}
	return entity;
}

[[nodiscard]] std::string TextureDisplayName(const TextureKey& key) {
	std::string name{ key.value };
	auto slash{ name.find_last_of("/\\") };
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
	auto slash{ name.find_last_of("/\\") };
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

[[nodiscard]] const char* AutotileFormatName(PaintAutotileFormat format) {
	switch (format) {
		case PaintAutotileFormat::Classic15:  return "Classic 15";
		case PaintAutotileFormat::Blob47:	  return "Blob 47 (8 neighbor)";
		case PaintAutotileFormat::Subset16:	  return "4 neighbor / Subset 16";
		case PaintAutotileFormat::DualGrid16: return "Dual Grid (15 or 16)";
		case PaintAutotileFormat::Wang16:	  return "Wang 16";
	}
	return "Autotile";
}

struct AutotileAtlasLayout {
	V2_int block_size{};
	int source_count{};
};

[[nodiscard]] bool FitsAutotileBlock(V2_int sheet_size, V2_int block_size) {
	return sheet_size.IsPositive() && block_size.IsPositive() && sheet_size.x >= block_size.x &&
		   sheet_size.y >= block_size.y;
}

[[nodiscard]] std::optional<AutotileAtlasLayout> ResolveAutotileAtlasLayout(
	PaintAutotileFormat format, V2_int sheet_size
) {
	auto layout = [&](V2_int block_size, int source_count) -> std::optional<AutotileAtlasLayout> {
		if (!FitsAutotileBlock(sheet_size, block_size)) {
			return std::nullopt;
		}
		return AutotileAtlasLayout{
			.block_size	  = block_size,
			.source_count = source_count,
		};
	};

	switch (format) {
		case PaintAutotileFormat::Classic15:
			// Also accept 4x4 sheets with an explicit empty mask at slot zero.
			if (auto result{ layout({ 5, 3 }, 15) }) {
				return result;
			}
			if (auto result{ layout({ 3, 5 }, 15) }) {
				return result;
			}
			return layout({ 4, 4 }, 16);

		case PaintAutotileFormat::Blob47:
			// Canonical 47 blob atlas: 8x6 with the final cell unused.
			return layout({ 8, 6 }, 47);

		case PaintAutotileFormat::DualGrid16:
			// A 15 piece dual grid atlas has no empty variant. A 4x4 atlas may
			// additionally contain an explicit transparent zero mask.
			if (sheet_size == V2_int{ 5, 3 } || sheet_size == V2_int{ 3, 5 }) {
				return layout(sheet_size, 15);
			}
			[[fallthrough]];

		case PaintAutotileFormat::Subset16:
		case PaintAutotileFormat::Wang16:
			// Prefer the square 4x4 form, but accept common row major strips.
			if (auto result{ layout({ 4, 4 }, 16) }) {
				return result;
			}
			if (auto result{ layout({ 8, 2 }, 16) }) {
				return result;
			}
			if (auto result{ layout({ 2, 8 }, 16) }) {
				return result;
			}
			if (auto result{ layout({ 16, 1 }, 16) }) {
				return result;
			}
			return layout({ 1, 16 }, 16);
	}

	return std::nullopt;
}

[[nodiscard]] std::optional<V2_int> InferAutotileTileSize(
	PaintAutotileFormat format, V2_int texture_size
) {
	std::vector<V2_int> shapes;
	switch (format) {
		case PaintAutotileFormat::Classic15: shapes = { { 5, 3 }, { 3, 5 }, { 4, 4 } }; break;
		case PaintAutotileFormat::Blob47:	 shapes = { { 8, 6 } }; break;
		case PaintAutotileFormat::DualGrid16:
			shapes = { { 5, 3 }, { 3, 5 }, { 4, 4 }, { 8, 2 }, { 2, 8 } };
			break;
		case PaintAutotileFormat::Subset16:
		case PaintAutotileFormat::Wang16:
			shapes = { { 4, 4 }, { 8, 2 }, { 2, 8 }, { 16, 1 }, { 1, 16 } };
			break;
	}
	for (V2_int shape : shapes) {
		if (texture_size.x % shape.x != 0 || texture_size.y % shape.y != 0) {
			continue;
		}
		V2_int tile{ texture_size.x / shape.x, texture_size.y / shape.y };
		if (tile.IsPositive() && tile.x == tile.y) {
			return tile;
		}
	}
	return std::nullopt;
}

[[nodiscard]] V2_int AutotileSourceSlice(const AutotileAtlasLayout& layout, int source_index) {
	return {
		source_index % layout.block_size.x,
		source_index / layout.block_size.x,
	};
}

[[nodiscard]] int AutotileIndex(
	PaintAutotileFormat format, const std::function<bool(V2_int)>& occupied, V2_int cell
) {
	return AutotileSlot(
		format == PaintAutotileFormat::Blob47 ? TilemapAutotileFormat::Blob47
											  : TilemapAutotileFormat::Subset16,
		occupied, cell
	);
}

[[nodiscard]] EditorIcon PaintToolIcon(PaintTool tool) {
	switch (tool) {
		case PaintTool::Select:		return EditorIcon::Select;
		case PaintTool::Move:		return EditorIcon::Move;
		case PaintTool::Brush:		return EditorIcon::Brush;
		case PaintTool::Line:		return EditorIcon::Line;
		case PaintTool::Rectangle:	return EditorIcon::Rectangle;
		case PaintTool::Fill:		return EditorIcon::Fill;
		case PaintTool::Erase:		return EditorIcon::Erase;
		case PaintTool::Eyedropper: return EditorIcon::Eyedropper;
	}
	return EditorIcon::Select;
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

[[nodiscard]] bool IsImagePath(const path& filepath) {
	std::string ext{ filepath.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});

	return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".gif";
}

[[nodiscard]] std::optional<V2_int> FilenameTileDimensions(const path& filepath) {
	static const std::regex dimensions{ R"((?:_|-)([0-9]+)x([0-9]+)$)", std::regex::icase };
	std::smatch match;

	std::string stem{ filepath.stem().string() };

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

[[nodiscard]] std::string SourceGroupName(const path& filepath) {
	static const std::regex dimensions{ R"((?:_|-)[0-9]+x[0-9]+$)", std::regex::icase };
	std::string result{ std::regex_replace(filepath.stem().string(), dimensions, "") };
	return result.empty() ? "Tiles" : result;
}

struct TiledTilesetInfo {
	int tile_width{};
	int tile_height{};
	std::vector<std::string> images{};
};

[[nodiscard]] std::string ReadTextFile(const path& filepath) {
	std::ifstream file{ filepath, std::ios::binary };
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

[[nodiscard]] TiledTilesetInfo ParseTiledTileset(const path& filepath) {
	TiledTilesetInfo info;
	std::string text{ ReadTextFile(filepath) };
	if (text.empty()) {
		return info;
	}
	std::string ext{ filepath.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	if (ext == ".tsx") {
		info.tile_width =
			RegexInt(text, std::regex{ R"rx(tilewidth\s*=\s*"([0-9]+)")rx", std::regex::icase });
		info.tile_height =
			RegexInt(text, std::regex{ R"rx(tileheight\s*=\s*"([0-9]+)")rx", std::regex::icase });
		std::regex image{ R"rx(<image[^>]*\bsource\s*=\s*"([^"]+)")rx", std::regex::icase };
		for (std::sregex_iterator it{ text.begin(), text.end(), image }, end; it != end; ++it) {
			info.images.push_back((*it)[1].str());
		}
	} else {
		info.tile_width =
			RegexInt(text, std::regex{ R"rx("tilewidth"\s*:\s*([0-9]+))rx", std::regex::icase });
		info.tile_height =
			RegexInt(text, std::regex{ R"rx("tileheight"\s*:\s*([0-9]+))rx", std::regex::icase });
		std::regex image{ R"rx("image"\s*:\s*"([^"]+)")rx", std::regex::icase };
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

constexpr float kRecipeMinimumLabelWidth{ 116.0f };

[[nodiscard]] float RecipeLabelWidth() {
	static constexpr std::array labels{
		"Center Density",
		"Minimum Spacing",
		"Preview Opacity",
		"Random Rotation",
	};

	float width{ kRecipeMinimumLabelWidth };
	for (const char* label : labels) {
		width = std::max(width, ImGui::CalcTextSize(label).x + ImGui::GetStyle().ItemSpacing.x);
	}
	return std::ceil(width);
}

[[nodiscard]] float ComboPopupSideInset() {
	return std::max(0.0f, ImGui::GetStyle().WindowPadding.x - ImGui::GetStyle().FramePadding.x);
}

[[nodiscard]] float AvailableContentWidth(float right_inset = 0.0f) {
	return std::max(1.0f, ImGui::GetContentRegionAvail().x - right_inset);
}

void BeginRecipeFillRow(const char* label) {
	float start_x{ ImGui::GetCursorPosX() };

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::SameLine();
	ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), start_x + RecipeLabelWidth()));
}

void BeginRecipeFillField(const char* label, float right_inset = 0.0f) {
	BeginRecipeFillRow(label);
	ImGui::SetNextItemWidth(AvailableContentWidth(right_inset));
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
			bool selected{ entry.value == origin };
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
	ImVec2 min{ ImGui::GetCursorScreenPos() };
	ImGui::Dummy({ side, side });
	ImVec2 max{ min.x + side, min.y + side };

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
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

	std::string id{ "##PaintTool" + std::to_string(static_cast<int>(tool)) };
	if (DrawEditorIconButton(
			id.c_str(), PaintToolIcon(tool),
			EditorIconButtonOptions{
				.tooltip	   = tooltip.c_str(),
				.selected	   = tool == selected,
				.no_navigation = true,
			}
		)) {
		selected = tool;
	}
}

[[nodiscard]] float SteppedIntControlWidth() {
	const auto& style{ ImGui::GetStyle() };
	// Match the width of the Line Thickness control for every stepped integer control.
	constexpr const char* preview{ "32" };
	float input_width{
		std::ceil(ImGui::CalcTextSize(preview).x + style.FramePadding.x * 2.0f + 4.0f)
	};
	return ImGui::GetFrameHeight() * 2.0f + input_width + 4.0f;
}

void DrawSteppedIntControl(const char* id, int& value, int maximum, const char* tooltip) {
	ImGui::PushID(id);

	constexpr float spacing{ 2.0f };
	float button_size{ ImGui::GetFrameHeight() };
	float input_width{ SteppedIntControlWidth() - button_size * 2.0f - spacing * 2.0f };

	if (ImGui::Button("-", { button_size, button_size })) {
		value = std::max(1, value - 1);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::SetNextItemWidth(input_width);
	ImGui::DragInt("##Value", &value, 0.2f, 1, maximum, "%d");
	value = std::clamp(value, 1, maximum);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}

	ImGui::SameLine(0.0f, spacing);
	if (ImGui::Button("+", { button_size, button_size })) {
		value = std::min(maximum, value + 1);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", tooltip);
	}

	ImGui::PopID();
}

[[nodiscard]] float BrushShapeComboWidth() {
	const auto& style{ ImGui::GetStyle() };
	return std::ceil(
		ImGui::CalcTextSize("  Square").x + style.FramePadding.x * 2.0f + ImGui::GetFrameHeight()
	);
}

void DrawBrushShapeCombo(const char* id, PaintBrushShape& shape, float width = 0.0f) {
	float combo_width{ width > 0.0f ? width : BrushShapeComboWidth() };
	const char* preview{ shape == PaintBrushShape::Circle ? "  Circle" : "  Square" };
	ImGui::SetNextItemWidth(combo_width);
	ImGui::SetNextWindowSizeConstraints({ BrushShapeComboWidth(), 0.0f }, { combo_width, FLT_MAX });
	if (ImGui::BeginCombo(id, preview)) {
		for (PaintBrushShape choice : { PaintBrushShape::Circle, PaintBrushShape::Square }) {
			bool selected{ choice == shape };
			const char* label{ choice == PaintBrushShape::Circle ? "  Circle" : "  Square" };
			if (ImGui::Selectable(label, selected)) {
				shape = choice;
			}

			ImVec2 item_min{ ImGui::GetItemRectMin() };
			ImVec2 item_max{ ImGui::GetItemRectMax() };
			DrawEditorIcon(
				ImGui::GetWindowDrawList(),
				choice == PaintBrushShape::Circle ? EditorIcon::Brush : EditorIcon::Pencil,
				{ item_min.x + ImGui::GetStyle().FramePadding.x,
				  item_min.y + (item_max.y - item_min.y - 14.0f) * 0.5f },
				14.0f, ImGui::GetColorU32(ImGuiCol_Text)
			);
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
		}
		ImGui::EndCombo();
	}

	ImVec2 item_min{ ImGui::GetItemRectMin() };
	ImVec2 item_max{ ImGui::GetItemRectMax() };
	DrawEditorIcon(
		ImGui::GetWindowDrawList(),
		shape == PaintBrushShape::Circle ? EditorIcon::Brush : EditorIcon::Pencil,
		{ item_min.x + ImGui::GetStyle().FramePadding.x,
		  item_min.y + (item_max.y - item_min.y - 14.0f) * 0.5f },
		14.0f, ImGui::GetColorU32(ImGuiCol_Text)
	);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Brush shape: %s", shape == PaintBrushShape::Circle ? "Circle" : "Square"
		);
	}
}

// Draw a paint option combo with the standard arrow.
bool DrawPaintCombo(const char* id, int& current, const char* items) {
	const char* label{ items };
	for (int index{}; index < current && *label; ++index) {
		label += std::strlen(label) + 1;
	}

	bool changed{};
	if (ImGui::BeginCombo(id, label)) {
		int index{};
		for (const char* item{ items }; *item; item += std::strlen(item) + 1, ++index) {
			bool selected{ index == current };
			if (ImGui::Selectable(item, selected)) {
				current = index;
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

} // namespace

void PaintEditor::ClearExclusionMask(EditorContext& ctx, Scene& scene) {
	if (Tilemap map{ ResolveTargetTilemap(scene) }) {
		auto before{ map.GetData() };
		map.ClearExclusionMask();
		auto after{ map.GetData() };
		if (before.exclusion_mask == after.exclusion_mask) {
			return;
		}
		UUID uuid{ map.Get<UUID>() };
		Scene* target_scene{ &scene };
		auto apply = [target_scene, uuid](const ::ptgn::impl::TilemapData& data) {
			if (Entity entity{ target_scene->GetEntity(uuid) }; entity && IsTilemap(entity)) {
				entity.Get<::ptgn::impl::TilemapData>() = data;
			}
		};
		PushPaintUndo(
			ctx, scene, "Clear Exclusion Mask", [apply, before]() { apply(before); },
			[apply, after]() { apply(after); }
		);
		return;
	}

	auto& project{ ctx.project_state.paint };
	auto before{ GetEntityExclusionCells(project, scene.GetTag()) };
	if (before.empty()) {
		return;
	}
	SetEntityExclusionCells(project, scene.GetTag(), {});
	EditorContext* context{ &ctx };
	std::string scene_key{ scene.GetTag() };
	PushPaintUndo(
		ctx, scene, "Clear Exclusion Mask",
		[context, scene_key, before]() {
			SetEntityExclusionCells(context->project_state.paint, scene_key, before);
		},
		[context, scene_key]() {
			SetEntityExclusionCells(context->project_state.paint, scene_key, {});
		}
	);
}

void PaintEditor::SetTool(PaintTool tool) {
	if (tool_ == tool) {
		return;
	}
	tool_				   = tool;
	stroke_				   = {};
	move_				   = {};
	selection_drag_start_  = {};
	selection_drag_active_ = false;
}

void PaintEditor::SetTargetTilemap(std::optional<UUID> uuid) {
	if (target_tilemap_ == uuid) {
		return;
	}
	target_tilemap_	  = uuid;
	selected_tilemap_ = uuid;
	selected_tile_cells_.clear();
	selected_entities_.clear();
	selected_generator_.reset();
	selection_drag_start_  = {};
	selection_drag_active_ = false;
}

bool PaintEditor::IsTileMode(const Scene& scene) const {
	return static_cast<bool>(ResolveTargetTilemap(scene));
}

Tilemap PaintEditor::ResolveTargetTilemap(Scene& scene) {
	return std::as_const(*this).ResolveTargetTilemap(scene);
}

Tilemap PaintEditor::ResolveTargetTilemap(const Scene& scene) const {
	if (!target_tilemap_) {
		return {};
	}
	Entity entity{ scene.GetEntity(*target_tilemap_) };
	return entity && IsTilemap(entity) ? Tilemap{ entity } : Tilemap{};
}

void PaintEditor::SyncHierarchySelection(EditorContext& ctx, Scene& scene) {
	Entity selected{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	if (!selected || &selected.GetScene() != &scene || IsTilemap(selected)) {
		return;
	}

	UUID uuid{ selected.Get<UUID>() };
	bool already_selected{ (IsPaintGenerator(selected) && selected_generator_ == uuid) ||
						   (!IsPaintGenerator(selected) &&
							std::ranges::contains(selected_entities_, uuid)) };
	if (already_selected) {
		return;
	}

	selected_entities_.clear();
	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();
	selection_drag_start_  = {};
	selection_drag_active_ = false;

	if (IsPaintGenerator(selected)) {
		selected_generator_ = uuid;
	} else if (!IsProtectedSceneEntity(scene, selected)) {
		selected_entities_.push_back(uuid);
	}
}

void PaintEditor::ValidateSceneState(Scene& scene) {
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
	selection_drag_start_  = {};
	selection_drag_active_ = false;
}

bool PaintEditor::HasSelection() const {
	return selected_generator_.has_value() || !selected_entities_.empty() ||
		   !selected_tile_cells_.empty();
}

bool PaintEditor::DeleteSelection(EditorContext& ctx, Scene& scene) {
	if (active_brush_generator_.has_value() && !stroke_.active) {
		CancelActiveBrushGenerator(ctx, scene, true);
		return true;
	}

	if (selected_generator_.has_value()) {
		Entity generator{ scene.GetEntity(*selected_generator_) };
		if (!generator || !IsPaintGenerator(generator)) {
			selected_generator_.reset();
			return false;
		}

		DeletePaintEntity(ctx, generator);
		ClearSelection();
		return true;
	}

	if (selected_entities_.size() == 1) {
		Entity entity{ scene.GetEntity(selected_entities_.front()) };
		if (!entity || IsProtectedSceneEntity(scene, entity) || IsTilemap(entity) ||
			IsPaintGenerator(entity)) {
			return false;
		}

		DeletePaintEntity(ctx, entity);
		ClearSelection();
		return true;
	}

	if (!selected_entities_.empty()) {
		struct DeletedSelectionEntity {
			SerializedEntity entity{};
			std::optional<UUID> parent{};
			std::optional<Transform> world_transform{};
		};

		std::vector<DeletedSelectionEntity> deleted;
		deleted.reserve(selected_entities_.size());

		for (UUID uuid : selected_entities_) {
			Entity entity{ scene.GetEntity(uuid) };
			if (!entity || IsProtectedSceneEntity(scene, entity) || IsTilemap(entity) ||
				IsPaintGenerator(entity)) {
				continue;
			}

			bool selected_ancestor{};
			Entity parent{ HasParent(entity) ? GetParent(entity) : Entity{} };
			while (parent) {
				if (parent.Has<UUID>() &&
					std::ranges::contains(selected_entities_, parent.Get<UUID>())) {
					selected_ancestor = true;
					break;
				}
				parent = HasParent(parent) ? GetParent(parent) : Entity{};
			}
			if (selected_ancestor) {
				continue;
			}

			std::optional<UUID> parent_uuid;
			std::optional<Transform> world_transform;
			if (HasParent(entity)) {
				Entity entity_parent{ GetParent(entity) };
				if (entity_parent && entity_parent.Has<UUID>()) {
					parent_uuid = entity_parent.Get<UUID>();
				}
				if (entity.Has<Transform>()) {
					world_transform = GetWorldTransform(entity);
				}
			}

			deleted.push_back(
				DeletedSelectionEntity{
					.entity			 = SerializeEntity(entity),
					.parent			 = parent_uuid,
					.world_transform = world_transform,
				}
			);
		}

		if (deleted.empty()) {
			return false;
		}

		const std::vector<UUID> selected_before{ selected_entities_ };
		const EditorSelection editor_selection_before{ ctx.local.selection };

		for (const auto& item : deleted) {
			if (!item.entity.uuid.has_value()) {
				continue;
			}
			if (Entity entity{ scene.GetEntity(*item.entity.uuid) }) {
				entity.Destroy();
			}
		}
		scene.Refresh();

		ClearSelection();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		const EditorSelection editor_selection_after{ ctx.local.selection };

		Scene* scene_ptr{ &scene };
		PaintEditor* paint{ this };
		EditorContext* context{ &ctx };

		PushPaintUndo(
			ctx, scene, "Delete Selection",
			[scene_ptr, paint, context, deleted, selected_before,
			 editor_selection_before]() mutable {
				for (const auto& item : deleted) {
					if (item.entity.uuid.has_value() && scene_ptr->GetEntity(*item.entity.uuid)) {
						continue;
					}
					RestoreEntityTree(*scene_ptr, item.entity);
				}
				scene_ptr->Refresh();

				for (const auto& item : deleted) {
					if (!item.entity.uuid.has_value() || !item.parent.has_value()) {
						continue;
					}
					Entity entity{ scene_ptr->GetEntity(*item.entity.uuid) };
					Entity parent{ scene_ptr->GetEntity(*item.parent) };
					if (!entity || !parent) {
						continue;
					}
					SetParent(entity, parent);
					if (item.world_transform.has_value() && entity.Has<Transform>()) {
						SetWorldTransform(entity, *item.world_transform);
					}
				}
				scene_ptr->Refresh();

				paint->ClearSelection();
				paint->selected_entities_ = selected_before;
				ApplyEditorSelection(*context, editor_selection_before);
			},
			[scene_ptr, paint, context, deleted, editor_selection_after]() mutable {
				for (const auto& item : deleted) {
					if (!item.entity.uuid.has_value()) {
						continue;
					}
					if (Entity entity{ scene_ptr->GetEntity(*item.entity.uuid) }) {
						entity.Destroy();
					}
				}
				scene_ptr->Refresh();
				paint->ClearSelection();
				ApplyEditorSelection(*context, editor_selection_after);
			}
		);

		ctx.local.state.is_dirty = true;
		return true;
	}

	if (selected_tilemap_.has_value() && !selected_tile_cells_.empty()) {
		Entity entity{ scene.GetEntity(*selected_tilemap_) };
		if (!entity || !IsTilemap(entity)) {
			selected_tilemap_.reset();
			selected_tile_cells_.clear();
			return false;
		}

		Tilemap map{ entity };
		UUID map_uuid{ map.Get<UUID>() };
		const auto before{ map.GetData() };
		const std::vector<V2_int> selected_cells_before{ selected_tile_cells_ };
		const EditorSelection editor_selection_before{ ctx.local.selection };
		std::vector<std::pair<V2_int, std::uint64_t>> removed_terrain;
		bool changed{};

		for (V2_int cell : selected_tile_cells_) {
			if (const auto terrain{ map.GetTerrainRuleset(cell) }) {
				map.SetTerrainRuleset(cell, std::nullopt);
				removed_terrain.emplace_back(cell, *terrain);
				changed = true;
			} else if (map.FindTile(cell)) {
				map.EraseTile(cell);
				changed = true;
			}
		}

		for (const auto& [cell, ruleset_id] : removed_terrain) {
			if (const PaintAutotileRuleSet* rules{ FindAutotileRuleSet(ruleset_id) }) {
				RecomputeAutotileAround(map, cell, *rules);
			}
		}

		if (!changed) {
			return false;
		}

		const auto after{ map.GetData() };
		selected_tilemap_.reset();
		selected_tile_cells_.clear();
		const EditorSelection editor_selection_after{ ctx.local.selection };

		Scene* scene_ptr{ &scene };
		PaintEditor* paint{ this };
		EditorContext* context{ &ctx };

		PushPaintUndo(
			ctx, scene, "Delete Tiles",
			[scene_ptr, paint, context, map_uuid, before, selected_cells_before,
			 editor_selection_before]() mutable {
				if (Entity map_entity{ scene_ptr->GetEntity(map_uuid) };
					map_entity && map_entity.Has<::ptgn::impl::TilemapData>()) {
					map_entity.Get<::ptgn::impl::TilemapData>() = before;
					paint->selected_tilemap_					= map_uuid;
					paint->selected_tile_cells_					= selected_cells_before;
				}
				ApplyEditorSelection(*context, editor_selection_before);
			},
			[scene_ptr, paint, context, map_uuid, after, editor_selection_after]() mutable {
				if (Entity map_entity{ scene_ptr->GetEntity(map_uuid) };
					map_entity && map_entity.Has<::ptgn::impl::TilemapData>()) {
					map_entity.Get<::ptgn::impl::TilemapData>() = after;
				}
				paint->selected_tilemap_.reset();
				paint->selected_tile_cells_.clear();
				ApplyEditorSelection(*context, editor_selection_after);
			}
		);

		ctx.local.state.is_dirty = true;
		return true;
	}

	return false;
}

void PaintEditor::DrawViewportToolButtons(EditorContext& ctx) {
	EnsureLocalState(ctx);

	auto* scene{ ctx.editor.GetSceneListPanel().GetSelectedScene() };
	if (!scene) {
		return;
	}
	ValidateSceneState(*scene);
	SyncHierarchySelection(ctx, *scene);

	bool tile_mode{ IsTileMode(*scene) };

	static constexpr std::array tools{
		PaintTool::Select,	  PaintTool::Move, PaintTool::Brush, PaintTool::Line,
		PaintTool::Rectangle, PaintTool::Fill, PaintTool::Erase, PaintTool::Eyedropper,
	};

	for (std::size_t i{}; i < tools.size(); ++i) {
		if (i != 0) {
			ImGui::SameLine();
		}

		PaintTool next{ tool_ };
		DrawToolButton(tools[i], next);
		if (next != tool_) {
			SetTool(next);
		}
	}

	ImGui::SameLine();
	ImGui::PushID("PaintGridToolbar");

	if (DrawEditorIconButton(
			"##Grid", EditorIcon::Grid,
			EditorIconButtonOptions{
				.tooltip	   = "Grid settings",
				.selected	   = grid_visible_,
				.no_navigation = true,
			}
		)) {
		ImGui::OpenPopup("PaintGridSettingsPopup");
	}

	ImGui::SetNextWindowSizeConstraints(ImVec2{ 360.0f, 0.0f }, ImVec2{ FLT_MAX, FLT_MAX });

	if (ImGui::BeginPopup("PaintGridSettingsPopup")) {
		ImGui::SeparatorText("Grid");

		BeginRecipeFillRow("Visible");
		if (DrawEditorIconButton(
				"##GridVisible", grid_visible_ ? EditorIcon::Visible : EditorIcon::Hidden,
				EditorIconButtonOptions{
					.tooltip	   = grid_visible_ ? "Hide" : "Show",
					.muted		   = !grid_visible_,
					.no_navigation = true,
					.icon_extent   = ImGui::GetTextLineHeight() + 2.0f,
				}
			)) {
			grid_visible_ = !grid_visible_;
		}

		if (!tile_mode) {
			float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			float lock_width{ ImGui::GetFrameHeight() };

			BeginRecipeFillRow("Cell Size");

			float value_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x) };
			float pair_width{ std::max(1.0f, value_width - lock_width - spacing * 2.0f) };
			float field_width{ std::max(1.0f, pair_width * 0.5f) };

			float width{ entity_grid_size_.x };
			float height{ entity_grid_size_.y };

			ImGui::SetNextItemWidth(field_width);
			bool width_changed{
				ImGui::DragFloat("##GridWidth", &width, 0.25f, 1.0f, 4096.0f, "W: %.0f")
			};

			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			bool height_changed{
				ImGui::DragFloat("##GridHeight", &height, 0.25f, 1.0f, 4096.0f, "H: %.0f")
			};

			ImGui::SameLine(0.0f, spacing);
			const char* aspect_lock_tooltip{ grid_aspect_locked_ ? "Unlock grid aspect ratio"
																 : "Lock grid aspect ratio" };
			if (DrawEditorIconButton(
					"##GridAspectLock",
					grid_aspect_locked_ ? EditorIcon::Locked : EditorIcon::Unlocked,
					EditorIconButtonOptions{
						.tooltip	   = aspect_lock_tooltip,
						.muted		   = !grid_aspect_locked_,
						.no_navigation = true,
						.icon_extent   = ImGui::GetTextLineHeight() + 2.0f,
					}
				)) {
				grid_aspect_locked_ = !grid_aspect_locked_;
				if (grid_aspect_locked_) {
					grid_locked_aspect_ = entity_grid_size_.x / std::max(1.0f, entity_grid_size_.y);
				}
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
			BeginRecipeFillField("Offset");
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
			BeginRecipeFillField("Color");
			if (DrawColorEdit(ctx, "##GridMinorColor", color)) {
				ColorToGridArray(color, grid_minor_color_);
			}
		}
		BeginRecipeFillField("Thickness");
		ImGui::DragFloat(
			"##GridMinorThickness", &grid_minor_thickness_, 0.05f, 0.25f, 8.0f, "%.2f"
		);

		ImGui::SeparatorText("Major Lines");
		BeginRecipeFillField("Every");
		ImGui::DragInt("##GridMajorEvery", &grid_major_every_, 0.2f, 1, 128);
		grid_major_every_ = std::max(1, grid_major_every_);

		{
			Color color{ GridArrayToColor(grid_major_color_) };
			BeginRecipeFillField("Color");
			if (DrawColorEdit(ctx, "##GridMajorColor", color)) {
				ColorToGridArray(color, grid_major_color_);
			}
		}
		BeginRecipeFillField("Thickness");
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

	bool tile_mode{ IsTileMode(*scene) };

	DrawBrushSettingsToolbar(ctx, *scene, tile_mode);
	StoreLocalState(ctx);
	return true;
}

void PaintEditor::DrawBrushSettingsToolbar(EditorContext& ctx, Scene& scene, bool tile_mode) {
	ImGui::PushID("PaintToolOptions");

	bool in_more_popup{};
	float more_popup_inset{};

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
			width	 = std::max(width, compact_width(preview, ImGui::GetFrameHeight()));
		}
		return width;
	};

	auto combo = [&](const char* id, const char* label, int& value, const char* items, int count,
					 float width = 0.0f) {
		std::string preview{ in_more_popup ? "" : label };
		if (!preview.empty()) {
			preview += ": ";
		}
		preview += item_at(items, value);

		ImGui::SetNextItemWidth(
			in_more_popup ? AvailableContentWidth(more_popup_inset)
						  : (width > 0.0f ? width : combo_width(label, items, count))
		);

		bool changed{};
		if (ImGui::BeginCombo(id, preview.c_str())) {
			for (int i{}; i < count; ++i) {
				bool selected{ i == value };
				if (ImGui::Selectable(item_at(items, i), selected)) {
					value	= i;
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

	float control_height{ ImGui::GetFrameHeight() };
	float combo_arrow_width{ ImGui::GetFrameHeight() };
	static constexpr std::array source_names{ "Single", "Weighted", "Checkered", "Noise" };
	float source_combo_width{};
	for (const char* source_name : source_names) {
		source_combo_width = std::max(
			source_combo_width,
			compact_width(std::string{ "Source: " } + source_name, combo_arrow_width)
		);
	}
	float area_combo_width{ combo_width("Area", "Fill\0Outline\0Corners\0Random\0", 4) };
	float result_combo_width{ compact_width("Result: Generator", combo_arrow_width) };

	struct ToolbarOption {
		const char* label;
		float width;
		std::function<void()> draw;
	};

	std::vector<ToolbarOption> options;
	options.reserve(8);

	auto add_option = [&](const char* label, float width, std::function<void()> draw) {
		options.push_back({ label, width, std::move(draw) });
	};

	// The recipe settings stay in More even when all toolbar options fit.
	bool recipe_applies{};
	std::function<void(float)> draw_recipe_settings;
	std::function<void(float)> draw_extra_settings;

	auto draw_options = [&] {
		float spacing{ ImGui::GetStyle().ItemSpacing.x };
		float available{ std::max(0.0f, ImGui::GetContentRegionAvail().x - 2.0f) };
		float more_width{ compact_width("More", ImGui::GetFrameHeight()) };
		std::size_t inline_count{ options.size() };

		auto inline_width = [&](std::size_t count) {
			float width{};
			for (std::size_t i{}; i < count; ++i) {
				width += options[i].width;
			}
			if (count > 1) {
				width += spacing * static_cast<float>(count - 1);
			}
			return width;
		};

		bool show_more{ recipe_applies || static_cast<bool>(draw_extra_settings) ||
						inline_width(inline_count) > available };
		if (show_more) {
			while (inline_count > 0 &&
				   inline_width(inline_count) + spacing + more_width > available) {
				--inline_count;
			}
		}

		for (std::size_t i{}; i < inline_count; ++i) {
			if (i != 0) {
				ImGui::SameLine(0.0f, spacing);
			}
			options[i].draw();
		}

		if (show_more) {
			if (inline_count != 0) {
				ImGui::SameLine(0.0f, spacing);
			}

			// Keep the More control at its measured width instead of shrinking its label.
			ImGui::SetNextItemWidth(more_width);
			ImGui::SetNextWindowSizeConstraints(ImVec2{ 330.0f, 0.0f }, ImVec2{ 460.0f, 520.0f });
			if (ImGui::BeginCombo("##PaintMore", "More")) {
				more_popup_inset = ComboPopupSideInset();
				ImGui::Indent(more_popup_inset);

				if (inline_count < options.size()) {
					ImGui::SeparatorText("Toolbar Options");
					in_more_popup = true;
					for (std::size_t i{ inline_count }; i < options.size(); ++i) {
						ImGui::PushID(static_cast<int>(i));
						BeginRecipeFillRow(options[i].label ? options[i].label : "");
						options[i].draw();
						ImGui::PopID();
					}
					in_more_popup = false;
				}

				if (draw_extra_settings) {
					if (inline_count < options.size()) {
						ImGui::Spacing();
					}
					draw_extra_settings(more_popup_inset);
				}

				if (draw_recipe_settings) {
					if (inline_count < options.size()) {
						ImGui::Spacing();
					}
					draw_recipe_settings(more_popup_inset);
				}

				ImGui::Unindent(more_popup_inset);
				ImGui::EndCombo();
			}
		}

		if (options.empty() && !show_more) {
			ImGui::Dummy({ 0.0f, control_height });
		}
	};

	auto add_diameter = [&](const char* id, int& diameter) {
		int* diameter_ptr{ &diameter };
		add_option("Diameter", SteppedIntControlWidth(), [diameter_ptr, id] {
			DrawSteppedIntControl(id, *diameter_ptr, 128, "Brush diameter");
		});
	};

	auto add_shape = [&](const char* id, PaintBrushShape& shape) {
		PaintBrushShape* shape_ptr{ &shape };
		add_option(nullptr, BrushShapeComboWidth(), [&, shape_ptr, id] {
			DrawBrushShapeCombo(id, *shape_ptr, BrushShapeComboWidth());
		});
	};

	if (tool_ == PaintTool::Select) {
		add_option(
			"Selection Mode", combo_width("Mode", "Click + Marquee\0Selection Brush\0", 2), [&] {
				int mode{ static_cast<int>(select_mode_) };
				if (combo("##SelectMode", "Mode", mode, "Click + Marquee\0Selection Brush\0", 2)) {
					select_mode_		   = static_cast<PaintSelectMode>(mode);
					selection_drag_start_  = {};
					selection_drag_active_ = false;
				}
			}
		);

		if (select_mode_ == PaintSelectMode::Brush) {
			add_diameter("SelectionDiameter", selection_diameter_);
			add_shape("##SelectionShape", selection_brush_shape_);
		}

		add_option("Deselect", compact_width("Deselect"), [&] {
			bool has_selection{
				HasSelection() ||
				static_cast<bool>(ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity())
			};
			ImGui::BeginDisabled(!has_selection);
			if (ImGui::Button(
					"Deselect", { in_more_popup ? AvailableContentWidth(more_popup_inset) : 0.0f,
								  control_height }
				)) {
				ClearSelection();
				ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
			}
			ImGui::EndDisabled();
		});

		draw_options();
		ImGui::PopID();
		return;
	}

	if (tool_ == PaintTool::Move) {
		add_option("Move", combo_width("Move", "Grid\0Free\0", 2), [&] {
			int snap{ static_cast<int>(move_snap_) };
			if (combo("##MoveMode", "Move", snap, "Grid\0Free\0", 2)) {
				move_snap_ = static_cast<PaintMoveSnapMode>(snap);
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Ctrl/Cmd temporarily inverts Grid/Free movement while dragging."
				);
			}
		});

		add_option("Snap to Grid", compact_width("Snap to Grid"), [&] {
			ImGui::BeginDisabled(!HasSelection());
			if (ImGui::Button(
					"Snap to Grid",
					{ in_more_popup ? AvailableContentWidth(more_popup_inset) : 0.0f,
					  control_height }
				)) {
				SnapSelectionToGrid(ctx, scene);
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"Snap the selected entity, generator, or tiles to the nearest grid coordinates."
				);
			}
		});

		draw_options();
		ImGui::PopID();
		return;
	}

	if (tool_ == PaintTool::Eyedropper) {
		draw_options();
		ImGui::PopID();
		return;
	}

	bool entity_mode{ !tile_mode };
	bool emits_content{ tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
						tool_ == PaintTool::Rectangle || tool_ == PaintTool::Fill };

	if (tool_ == PaintTool::Erase) {
		int erase_mode{ recipe_.operation == PaintBrushOperation::ExclusionMask ? 1 : 0 };
		const char* operations{ tile_mode ? "Tiles\0Exclusion Mask\0"
										  : "Entities\0Exclusion Mask\0" };
		add_option(
			"Erase Mode", combo_width("Mode", operations, 2),
			[&, erase_mode, operations]() mutable {
				if (combo("##EraseOperation", "Mode", erase_mode, operations, 2)) {
					recipe_.operation = erase_mode == 1 ? PaintBrushOperation::ExclusionMask
														: PaintBrushOperation::Paint;
				}
			}
		);
	} else if (emits_content) {
		if (tool_ == PaintTool::Fill) {
			if (recipe_.operation != PaintBrushOperation::Paint &&
				recipe_.operation != PaintBrushOperation::Replace &&
				recipe_.operation != PaintBrushOperation::Erase &&
				recipe_.operation != PaintBrushOperation::ExclusionMask) {
				recipe_.operation = PaintBrushOperation::Paint;
			}

			const char* operations{ "Paint\0Replace\0Erase\0Exclusion Mask\0" };
			add_option("Paint Mode", combo_width("Mode", operations, 4), [&, operations] {
				int mode{ recipe_.operation == PaintBrushOperation::Replace			? 1
						  : recipe_.operation == PaintBrushOperation::Erase			? 2
						  : recipe_.operation == PaintBrushOperation::ExclusionMask ? 3
																					: 0 };
				if (combo("##PaintOperation", "Mode", mode, operations, 4)) {
					recipe_.operation = mode == 1 ? PaintBrushOperation::Replace
									  : mode == 2 ? PaintBrushOperation::Erase
									  : mode == 3 ? PaintBrushOperation::ExclusionMask
												  : PaintBrushOperation::Paint;
				}
			});
		} else {
			if (recipe_.operation == PaintBrushOperation::Erase) {
				recipe_.operation = PaintBrushOperation::Paint;
			}
			const char* operations{ "Paint\0Replace\0Exclusion Mask\0" };
			add_option("Paint Mode", combo_width("Mode", operations, 3), [&, operations] {
				int mode{ recipe_.operation == PaintBrushOperation::ExclusionMask ? 2
						  : recipe_.operation == PaintBrushOperation::Replace	  ? 1
																				  : 0 };
				if (combo("##PaintOperation", "Mode", mode, operations, 3)) {
					recipe_.operation = mode == 2 ? PaintBrushOperation::ExclusionMask
									  : mode == 1 ? PaintBrushOperation::Replace
												  : PaintBrushOperation::Paint;
				}
			});
		}
	}

	bool source_applicable{ emits_content &&
							recipe_.operation != PaintBrushOperation::ExclusionMask &&
							recipe_.operation != PaintBrushOperation::Erase };

	if (source_applicable) {
		if (entity_mode && recipe_.source_kind == PaintSourceKind::Autotile) {
			recipe_.source_kind = PaintSourceKind::Single;
		}
		// A single paint source follows the current asset selection.
		if (recipe_.source_kind == PaintSourceKind::Single) {
			if (tile_mode) {
				if (const TileLibraryEntry* selected{ FindTileEntry(selected_tile_entry_id_) }) {
					tile_source_ = MakeTileSource(ctx, selected->texture, selected->slice);
				} // Keep a direct tile selection when browsing an autotile as slices.
			} else if (entity_mode) {
				auto selected_prefab{ ctx.editor.GetSceneHierarchyPanel().GetSelectedPrefab() };
				prefab_source_ = selected_prefab.value_or(PrefabKey{});
			}
		}

		add_option("Paint Source", source_combo_width, [&] {
			auto source_name = [](PaintSourceKind kind) -> const char* {
				switch (kind) {
					case PaintSourceKind::Single:		return "Single";
					case PaintSourceKind::WeightedSet:	return "Weighted";
					case PaintSourceKind::Checkerboard: return "Checkered";
					case PaintSourceKind::Autotile:		return "Single";
					case PaintSourceKind::Noise:		return "Noise";
				}
				return "Single";
			};

			std::string source_preview{ std::string{ in_more_popup ? "" : "Source: " } +
										source_name(recipe_.source_kind) };
			ImGui::SetNextItemWidth(
				in_more_popup ? AvailableContentWidth(more_popup_inset) : source_combo_width
			);

			bool noise_source{ recipe_.source_kind == PaintSourceKind::Noise };
			bool weighted_source{ recipe_.source_kind == PaintSourceKind::WeightedSet };

			float source_popup_min_width{ noise_source ? 720.0f : 420.0f };
			float source_popup_max_width{ noise_source ? 900.0f : 620.0f };

			if (weighted_source) {
				const std::string& selected_name{ tile_mode ? recipe_.weighted_tile_set_name
															: recipe_.weighted_prefab_set_name };
				const char* preview{ selected_name.empty() ? "<none>" : selected_name.c_str() };
				float preview_combo_width{ ImGui::CalcTextSize(preview).x +
										   ImGui::GetStyle().FramePadding.x * 2.0f +
										   ImGui::GetFrameHeight() };
				float add_width{ ImGui::CalcTextSize("Add Weighted Set").x +
								 ImGui::GetStyle().FramePadding.x * 2.0f };
				float delete_width{ ImGui::CalcTextSize("Delete Set").x +
									ImGui::GetStyle().FramePadding.x * 2.0f };
				float row_spacing{ ImGui::GetStyle().ItemSpacing.x * 2.0f };
				float popup_padding{ ImGui::GetStyle().WindowPadding.x * 2.0f +
									 ComboPopupSideInset() * 2.0f };

				source_popup_min_width = std::max(
					560.0f, popup_padding + RecipeLabelWidth() + preview_combo_width + add_width +
								delete_width + row_spacing
				);
				source_popup_max_width = std::max(720.0f, source_popup_min_width);
			}

			ImGui::SetNextWindowSizeConstraints(
				ImVec2{ source_popup_min_width, 0.0f }, ImVec2{ source_popup_max_width, 680.0f }
			);

			bool source_open{ ImGui::BeginCombo("##PaintSource", source_preview.c_str()) };
			bool source_hovered{ ImGui::IsItemHovered() };

			if (source_open) {
				float source_popup_inset{ ComboPopupSideInset() };
				ImGui::Indent(source_popup_inset);

				auto source_item = [&](PaintSourceKind kind, const char* label) {
					bool selected{ recipe_.source_kind == kind ||
								   (kind == PaintSourceKind::Single &&
									recipe_.source_kind == PaintSourceKind::Autotile) };
					if (ImGui::Selectable(
							label, selected, ImGuiSelectableFlags_DontClosePopups,
							ImVec2{ AvailableContentWidth(source_popup_inset), 0.0f }
						)) {
						recipe_.source_kind = kind;
						if (kind == PaintSourceKind::Single && tile_mode &&
							std::ranges::contains(
								autotile_library_, recipe_.autotile_texture,
								&AutotileLibraryEntry::texture
							) &&
							selected_tile_entry_id_.empty() && !tile_source_) {
							recipe_.source_kind = PaintSourceKind::Autotile;
						}
					}
					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				};

				source_item(PaintSourceKind::Single, "Single");
				source_item(PaintSourceKind::WeightedSet, "Weighted");
				source_item(PaintSourceKind::Checkerboard, "Checkered");
				source_item(PaintSourceKind::Noise, "Noise");

				if (recipe_.source_kind == PaintSourceKind::Single ||
					recipe_.source_kind == PaintSourceKind::Autotile) {
					ImGui::Separator();
					if (tile_mode) {
						if (const TileLibraryEntry* selected{
								FindTileEntry(selected_tile_entry_id_) }) {
							ImGui::TextDisabled("Using selected tile: %s", selected->name.c_str());
						} else if (recipe_.source_kind == PaintSourceKind::Autotile) {
							ImGui::TextDisabled("Using selected autotile tileset.");
						} else if (tile_source_) {
							ImGui::TextDisabled("Using selected atlas tile.");
						} else {
							ImGui::TextDisabled("Select a tile in the Tiles window.");
						}
					} else {
						auto selected_prefab{
							ctx.editor.GetSceneHierarchyPanel().GetSelectedPrefab()
						};
						if (selected_prefab.has_value()) {
							ImGui::TextDisabled(
								"Using selected prefab: %s",
								PrefabDisplayName(*selected_prefab).c_str()
							);
						} else {
							ImGui::TextDisabled("Select a prefab in the Prefabs window.");
						}
					}
				} else {
					ImGui::Separator();
					if (recipe_.source_kind == PaintSourceKind::Noise) {
						DrawNoiseRecipe(ctx, scene, tile_mode);
					} else {
						DrawExtendedSourceRecipe(ctx, scene, tile_mode);
					}
				}

				ImGui::Unindent(source_popup_inset);
				ImGui::EndCombo();
			}

			if (source_hovered) {
				ImGui::SetTooltip(
					"Single uses the current selection from the %s window. "
					"Other source types are configured inside this popup.",
					tile_mode ? "Tiles" : "Prefabs"
				);
			}
		});
	}

	if (tool_ == PaintTool::Brush || tool_ == PaintTool::Erase) {
		add_diameter("PaintDiameter", brush_diameter_);
		add_shape("##BrushShape", brush_shape_);
	}

	if (tool_ == PaintTool::Line) {
		add_option("Thickness", SteppedIntControlWidth(), [&] {
			DrawSteppedIntControl("LineThickness", line_thickness_, 32, "Line thickness");
		});
	}

	if (tool_ == PaintTool::Rectangle) {
		add_option("Area Mode", area_combo_width, [&] {
			int mode{ static_cast<int>(area_mode_) };
			if (combo(
					"##AreaMode", "Area", mode, "Fill\0Outline\0Corners\0Random\0", 4,
					area_combo_width
				)) {
				area_mode_ = static_cast<PaintAreaMode>(mode);
			}
		});

		if (area_mode_ == PaintAreaMode::Outline || area_mode_ == PaintAreaMode::Corners) {
			add_option("Thickness", SteppedIntControlWidth(), [&] {
				DrawSteppedIntControl(
					"AreaThickness", area_thickness_, 32,
					area_mode_ == PaintAreaMode::Corners ? "Corner thickness" : "Outline thickness"
				);
			});
		} else if (area_mode_ == PaintAreaMode::RandomFill) {
			add_option("Density", compact_width("Density: 1.00"), [&] {
				ImGui::SetNextItemWidth(
					in_more_popup ? AvailableContentWidth(more_popup_inset)
								  : compact_width("Density: 1.00")
				);
				ImGui::SliderFloat(
					"##AreaDensity", &recipe_.density, 0.0f, 1.0f,
					in_more_popup ? "%.2f" : "Density: %.2f"
				);
			});
		}
	}

	if (tool_ == PaintTool::Line && recipe_.operation == PaintBrushOperation::ExclusionMask) {
		draw_extra_settings = [&](float popup_inset) {
			ImGui::SeparatorText("Coverage");
			BeginRecipeFillField("Spacing", popup_inset);
			ImGui::DragInt("##LineSpacing", &line_spacing_, 0.2f, 1, 32, "%d");
			line_spacing_ = std::max(1, line_spacing_);
		};
	}

	recipe_applies = source_applicable;
	if (recipe_applies) {
		if (entity_mode) {
			float linked_width{ ImGui::CalcTextSize("Linked").x + 4.0f + ImGui::GetFrameHeight() +
								ImGui::GetStyle().ItemInnerSpacing.x };
			add_option("Linked Prefabs", linked_width, [&] {
				bool linked_label_hovered{};
				if (!in_more_popup) {
					ImGui::AlignTextToFramePadding();
					ImGui::TextUnformatted("Linked");
					linked_label_hovered = ImGui::IsItemHovered();
					ImGui::SameLine(0.0f, 4.0f);
				}
				ImGui::Checkbox("##PaintLinkedPrefabInstances", &recipe_.link_prefab_instances);
				if (linked_label_hovered || ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Keep painted prefab instances linked to their prefab asset in the editor. "
						"Disable to bake them into ordinary entities immediately."
					);
				}
			});
		}

		bool finite_generator_tool{ tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
									tool_ == PaintTool::Rectangle };
		if (finite_generator_tool && recipe_.operation == PaintBrushOperation::Paint) {
			add_option("Result", result_combo_width, [&] {
				int result{ static_cast<int>(recipe_.commit_mode) };
				if (combo(
						"##PaintResult", "Result", result, "Bake\0Generator\0", 2,
						result_combo_width
					)) {
					recipe_.commit_mode = static_cast<PaintCommitMode>(result);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Bake creates ordinary editable content. Generator stores the authored "
						"geometry and a captured recipe."
					);
				}
			});
		}

		draw_recipe_settings = [&](float right_inset) {
			ImGui::SeparatorText("Coverage");

			int coverage{ static_cast<int>(recipe_.coverage) };
			BeginRecipeFillField("Coverage", right_inset);
			if (DrawPaintCombo(
					"##PaintCoverage", coverage, "Solid\0Random Density\0Radial Falloff\0"
				)) {
				recipe_.coverage = static_cast<PaintCoverageMode>(coverage);
			}

			if (tool_ == PaintTool::Line) {
				BeginRecipeFillField("Spacing", right_inset);
				ImGui::DragInt("##LineSpacing", &line_spacing_, 0.2f, 1, 32, "%d");
				line_spacing_ = std::max(1, line_spacing_);
			}

			if (recipe_.coverage == PaintCoverageMode::RandomDensity) {
				BeginRecipeFillField("Density", right_inset);
				ImGui::SliderFloat("##PaintCoverageDensity", &recipe_.density, 0.0f, 1.0f, "%.2f");
			} else if (recipe_.coverage == PaintCoverageMode::RadialFalloff) {
				BeginRecipeFillField("Center Density", right_inset);
				ImGui::SliderFloat(
					"##PaintCoverageCenterDensity", &recipe_.density, 0.0f, 1.0f, "%.2f"
				);

				BeginRecipeFillField("Inner", right_inset);
				ImGui::SliderFloat(
					"##PaintCoverageRadialInner", &recipe_.radial_inner, 0.0f, 0.95f, "%.2f"
				);

				BeginRecipeFillField("Outer", right_inset);
				ImGui::SliderFloat(
					"##PaintCoverageRadialOuter", &recipe_.radial_outer, 0.05f, 1.0f, "%.2f"
				);
				recipe_.radial_outer = std::max(recipe_.radial_outer, recipe_.radial_inner + 0.01f);
			}

			if (tool_ == PaintTool::Line && entity_mode) {
				BeginRecipeFillRow("Align to Line");
				ImGui::Checkbox("##PaintAlignEntitiesToLine", &line_align_rotation_);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"Rotate newly placed prefabs to match the authored line direction."
					);
				}
			}

			if (tile_mode && recipe_.source_kind != PaintSourceKind::Autotile) {
				Tilemap target{ ResolveTargetTilemap(scene) };
				V2_float cell_size{ target ? target.GetData().cell_size : ActiveGridSize(scene) };

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
					BeginRecipeFillField("Placement", right_inset);
					if (DrawPaintCombo("##PaintTilePlacement", placement, "Grid\0Tile\0")) {
						recipe_.tile_placement = static_cast<PaintTilePlacementMode>(placement);
					}

					BeginRecipeFillField("Origin", right_inset);
					DrawOriginCombo("##PaintTileOrigin", recipe_.tile_origin);
				} else {
					recipe_.tile_placement = PaintTilePlacementMode::Tile;
				}

				recipe_.avoid_exclusion_mask = true;
			}

			if (entity_mode) {
				bool source_matches_grid{ false };
				if (prefab_source_) {
					auto& assets{ ctx.editor.GetAssetManager() };
					if (assets.EnsurePrefabResident(prefab_source_)) {
						auto prefab{
							::ptgn::impl::AssetAccessor{ assets }.Get<Prefab>(prefab_source_)
						};
						json root_json{ prefab.get().root };
						bool explicit_size{};
						if (root_json.contains("components") &&
							root_json["components"].is_object()) {
							for (auto it{ root_json["components"].begin() };
								 it != root_json["components"].end(); ++it) {
								std::string key{ ToLower(it.key()) };
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
					BeginRecipeFillField("Origin", right_inset);
					DrawOriginCombo("##PaintEntityOrigin", recipe_.entity_origin);
				} else {
					recipe_.entity_origin = Origin::Center;
				}

				ImGui::SeparatorText("Transform");
				BeginRecipeFillRow("Random Rotation");
				ImGui::Checkbox("##PaintRandomRotation", &recipe_.random_rotation);
				if (recipe_.random_rotation) {
					BeginRecipeFillField("Rotation", right_inset);
					ImGui::DragFloatRange2(
						"Rotation##PaintRotationRange", &recipe_.rotation_min,
						&recipe_.rotation_max, 0.5f, -3600.0f, 3600.0f, "%.0f", "%.0f"
					);
				}

				BeginRecipeFillRow("Random Scale");
				ImGui::Checkbox("##PaintRandomScale", &recipe_.random_scale);
				if (recipe_.random_scale) {
					BeginRecipeFillField("Scale", right_inset);
					ImGui::DragFloatRange2(
						"Scale##PaintScaleRange", &recipe_.scale_min, &recipe_.scale_max, 0.01f,
						0.01f, 8.0f, "%.2f", "%.2f"
					);
				}

				if (recipe_.coverage != PaintCoverageMode::Solid) {
					BeginRecipeFillField("Minimum Spacing", right_inset);
					ImGui::DragFloat(
						"##PaintMinimumSpacing", &recipe_.min_spacing, 0.25f, 0.0f, 4096.0f, "%.1f"
					);
				}
			}

			ImGui::SeparatorText("Procedural");
			if (ImGui::Button(
					"Create Infinite Generator",
					ImVec2{
						std::max(1.0f, ImGui::GetContentRegionAvail().x - right_inset),
						0.0f,
					}
				)) {
				CreateInfiniteGenerator(ctx, scene);
			}
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"Create a persistent infinite generator using the current source and coverage."
				);
			}
		};
	}

	draw_options();
	ImGui::PopID();
}

void PaintEditor::DrawNoiseThresholdGradient(EditorContext&, bool tile_mode) {
	auto& field{ recipe_.noise };
	if (field.thresholds.empty()) {
		PaintNoiseThreshold initial;
		initial.minimum = 0.0f;
		initial.maximum = 1.0f;
		initial.enabled = true;
		if (tile_mode && tile_source_) {
			initial.tile   = tile_source_;
			initial.origin = recipe_.tile_origin;
		} else if (!tile_mode && prefab_source_) {
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
			return tile_mode
					 ? (region.weighted_tile_set_name.empty() ? std::string{ "<weighted set>" }
															  : region.weighted_tile_set_name)
					 : (region.weighted_prefab_set_name.empty() ? std::string{ "<weighted set>" }
																: region.weighted_prefab_set_name);
		}
		if (tile_mode) {
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
		int maximum{ std::max(3, static_cast<int>(available / 7.0f)) };
		if (static_cast<int>(label.size()) <= maximum) {
			return label;
		}
		return label.substr(0, static_cast<std::size_t>(std::max(1, maximum - 3))) + "...";
	};

	normalize();
	float width{ std::max(260.0f, AvailableContentWidth(ComboPopupSideInset())) };
	float height{ 76.0f };
	ImVec2 p0{ ImGui::GetCursorScreenPos() };
	ImGui::InvisibleButton(
		"##recipe_noise_threshold_gradient", { width, height },
		ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
	);
	ImVec2 mouse{ ImGui::GetIO().MousePos };
	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	float bar_y0{ p0.y + 25.0f };
	float bar_y1{ p0.y + 47.0f };

	for (int i{}; i < 96; ++i) {
		float a{ static_cast<float>(i) / 96.0f };
		float b{ static_cast<float>(i + 1) / 96.0f };
		float value{ (a + b) * 0.5f };
		draw->AddRectFilled(
			{ p0.x + a * width, bar_y0 }, { p0.x + b * width + 1.0f, bar_y1 },
			ImGui::GetColorU32(ImVec4(value, value, value, 1.0f))
		);
	}
	draw->AddRect({ p0.x, bar_y0 }, { p0.x + width, bar_y1 }, ImGui::GetColorU32(ImGuiCol_Border));

	for (std::size_t i{}; i < field.thresholds.size(); ++i) {
		const auto& region{ field.thresholds[i] };
		float x0{ p0.x + region.minimum * width };
		float x1{ p0.x + region.maximum * width };
		if (region.enabled) {
			draw->AddRect(
				{ x0 + 1.0f, bar_y0 + 1.0f }, { x1 - 1.0f, bar_y1 - 1.0f },
				ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 0.95f)), 0.0f, 0, 2.0f
			);
		}
		std::string label{ truncated(source_label(region), std::max(0.0f, x1 - x0 - 4.0f)) };
		if (!label.empty()) {
			ImVec2 size{ ImGui::CalcTextSize(label.c_str()) };
			float x{ std::clamp((x0 + x1 - size.x) * 0.5f, p0.x, p0.x + width - size.x) };
			float y{ (i % 2 == 0) ? p0.y + 3.0f : bar_y1 + 6.0f };
			draw->AddText(
				{ x, y },
				ImGui::GetColorU32(region.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
				label.c_str()
			);
		}
	}

	for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
		float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
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
	ImGuiID widget_id{ ImGui::GetID("##recipe_noise_threshold_gradient") };
	auto nearest_boundary = [&] {
		int nearest{ -1 };
		float best{ 9.0f };
		for (int i{}; i + 1 < static_cast<int>(field.thresholds.size()); ++i) {
			float x{ p0.x + field.thresholds[static_cast<std::size_t>(i)].maximum * width };
			float distance{ std::abs(mouse.x - x) };
			if (distance < best) {
				best	= distance;
				nearest = i;
			}
		}
		return nearest;
	};

	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			dragging_id		  = widget_id;
			dragging_boundary = nearest;
		} else {
			split(std::clamp((mouse.x - p0.x) / width, 0.01f, 0.99f));
		}
	}
	if (dragging_id == widget_id && dragging_boundary >= 0 &&
		ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		float value{ std::clamp((mouse.x - p0.x) / width, 0.0f, 1.0f) };
		float low{ field.thresholds[static_cast<std::size_t>(dragging_boundary)].minimum + 0.005f };
		float high{ field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].maximum -
					0.005f };
		float boundary{ std::clamp(value, low, high) };
		field.thresholds[static_cast<std::size_t>(dragging_boundary)].maximum	  = boundary;
		field.thresholds[static_cast<std::size_t>(dragging_boundary + 1)].minimum = boundary;
	}
	if (dragging_id == widget_id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
		dragging_id		  = 0;
		dragging_boundary = -1;
	}
	if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		int nearest{ nearest_boundary() };
		if (nearest >= 0) {
			remove_boundary(nearest);
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip(
			"Left click empty gradient space: split a range.\n"
			"Left drag a divider: move the threshold.\n"
			"Right click a divider: remove it and merge its neighboring ranges."
		);
	}
}

void PaintEditor::DrawExtendedSourceRecipe(EditorContext& ctx, Scene&, bool tile_mode) {
	if (recipe_.source_kind == PaintSourceKind::WeightedSet) {
		if (tile_mode) {
			DrawWeightedTileSetEditor(ctx);
		} else {
			DrawWeightedPrefabSetEditor(ctx);
		}
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Checkerboard) {
		ImGui::SeparatorText("Checkerboard Sources");

		if (tile_mode) {
			std::optional<PaintTileSource> primary{
				tile_source_ ? std::optional<PaintTileSource>{ tile_source_ } : std::nullopt
			};

			BeginRecipeFillField("Primary", ComboPopupSideInset());
			if (DrawTileSourceCombo(ctx, "##PrimaryCheckerTile", primary)) {
				tile_source_ = primary.value_or(PaintTileSource{});
			}

			BeginRecipeFillField("Secondary", ComboPopupSideInset());
			DrawTileSourceCombo(ctx, "##SecondaryCheckerTile", recipe_.checker_tile);
		} else {
			std::optional<PrefabKey> primary{ prefab_source_
												  ? std::optional<PrefabKey>{ prefab_source_ }
												  : std::nullopt };

			BeginRecipeFillField("Primary", ComboPopupSideInset());
			if (DrawPrefabSourceCombo(ctx, "##PrimaryCheckerPrefab", primary)) {
				prefab_source_ = primary.value_or(PrefabKey{});
			}

			BeginRecipeFillField("Secondary", ComboPopupSideInset());
			DrawPrefabSourceCombo(ctx, "##SecondaryCheckerPrefab", recipe_.checker_prefab);
		}
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Autotile && tile_mode) {
		if (auto entry{ std::ranges::find(
				autotile_library_, recipe_.autotile_texture, &AutotileLibraryEntry::texture
			) };
			entry != autotile_library_.end()) {
			ImGui::TextDisabled(
				"Selected tileset: %s (%s)", entry->name.c_str(), AutotileFormatName(entry->format)
			);
		}
	}
}

void PaintEditor::DrawNoiseRecipe(EditorContext& ctx, Scene&, bool tile_mode) {
	ImGui::PushID("PaintNoiseSettings");
	ImGui::SeparatorText("Noise Source");

	float source_popup_inset{ ComboPopupSideInset() };

	int type_index{};
	if (recipe_.noise.type == NoiseType::Simplex) {
		type_index = 1;
	} else if (recipe_.noise.type == NoiseType::Value) {
		type_index = 2;
	}

	BeginRecipeFillField("Type", source_popup_inset);
	if (DrawPaintCombo("##PaintNoiseType", type_index, "Perlin\0Simplex\0Value\0")) {
		recipe_.noise.type = type_index == 0 ? NoiseType::Perlin
						   : type_index == 1 ? NoiseType::Simplex
											 : NoiseType::Value;
	}

	BeginRecipeFillField("Seed", source_popup_inset);
	ImGui::DragInt("##PaintNoiseSeed", &recipe_.noise.seed, 1.0f);

	BeginRecipeFillField("Frequency", source_popup_inset);
	ImGui::DragFloat(
		"##PaintNoiseFrequency", &recipe_.noise.frequency, 0.001f, 0.0001f, 1.0f, "%.4f",
		ImGuiSliderFlags_AlwaysClamp
	);

	BeginRecipeFillField("Octaves", source_popup_inset);
	ImGui::DragInt("##PaintNoiseOctaves", &recipe_.noise.octaves, 0.2f, 1, 12);

	BeginRecipeFillField("Lacunarity", source_popup_inset);
	ImGui::DragFloat(
		"##PaintNoiseLacunarity", &recipe_.noise.lacunarity, 0.02f, 1.0f, 8.0f, "%.2f",
		ImGuiSliderFlags_AlwaysClamp
	);

	BeginRecipeFillField("Persistence", source_popup_inset);
	ImGui::SliderFloat("##PaintNoisePersistence", &recipe_.noise.persistence, 0.0f, 1.0f, "%.2f");

	float offset[2]{
		recipe_.noise.offset.x,
		recipe_.noise.offset.y,
	};
	BeginRecipeFillField("Noise Offset", source_popup_inset);
	if (ImGui::DragFloat2("##PaintNoiseOffset", offset, 0.25f)) {
		recipe_.noise.offset = { offset[0], offset[1] };
	}

	BeginRecipeFillRow("Noise Preview");
	ImGui::Checkbox("##PaintNoisePreviewEnabled", &recipe_.show_noise_preview);
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Overlay the raw grayscale noise only inside generator/paint geometry.");
	}

	if (recipe_.show_noise_preview) {
		BeginRecipeFillField("Preview Opacity", source_popup_inset);
		ImGui::SliderFloat(
			"##PaintNoisePreviewOpacity", &recipe_.noise_preview_alpha, 0.0f, 1.0f, "%.2f"
		);
	}

	ImGui::SeparatorText("Noise Thresholds");
	DrawNoiseThresholdGradient(ctx, tile_mode);

	float row_height{ ImGui::GetFrameHeight() };
	float range_width{ std::ceil(ImGui::CalcTextSize("0.000 - 0.000").x) };
	float kind_width{ 118.0f };
	float origin_width{ 112.0f };
	float table_width{ AvailableContentWidth(source_popup_inset) };

	int remove_region{ -1 };
	if (ImGui::BeginTable(
			"##NoiseRanges", 6, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings,
			ImVec2{ table_width, 0.0f }
		)) {
		ImGui::TableSetupColumn("Enabled", ImGuiTableColumnFlags_WidthFixed, row_height);
		ImGui::TableSetupColumn("Range", ImGuiTableColumnFlags_WidthFixed, range_width);
		ImGui::TableSetupColumn("Kind", ImGuiTableColumnFlags_WidthFixed, kind_width);
		ImGui::TableSetupColumn("Source", ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("Origin", ImGuiTableColumnFlags_WidthFixed, origin_width);
		ImGui::TableSetupColumn("Remove", ImGuiTableColumnFlags_WidthFixed, row_height);

		for (int i{}; i < static_cast<int>(recipe_.noise.thresholds.size()); ++i) {
			auto& region{ recipe_.noise.thresholds[static_cast<std::size_t>(i)] };

			ImGui::PushID(i);
			ImGui::TableNextRow(ImGuiTableRowFlags_None, row_height);

			ImGui::TableSetColumnIndex(0);
			ImGui::Checkbox("##NoiseRangeEnabled", &region.enabled);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Enable or disable output from this noise range.");
			}

			ImGui::TableSetColumnIndex(1);
			ImGui::AlignTextToFramePadding();
			ImGui::TextDisabled("%.3f - %.3f", region.minimum, region.maximum);

			ImGui::TableSetColumnIndex(2);
			int kind{ !region.enabled									   ? 0
					  : region.source_kind == PaintSourceKind::WeightedSet ? 2
																		   : 1 };
			ImGui::SetNextItemWidth(-FLT_MIN);
			if (DrawPaintCombo("##NoiseRangeKind", kind, "None\0Single\0Weighted Set\0")) {
				region.enabled = kind != 0;
				region.source_kind =
					kind == 2 ? PaintSourceKind::WeightedSet : PaintSourceKind::Single;
			}

			ImGui::TableSetColumnIndex(3);
			if (region.enabled) {
				ImGui::SetNextItemWidth(-FLT_MIN);
				if (tile_mode) {
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
			}

			ImGui::TableSetColumnIndex(4);
			if (region.enabled) {
				ImGui::SetNextItemWidth(-FLT_MIN);
				DrawOriginCombo("##NoiseRangeOrigin", region.origin);
			}

			ImGui::TableSetColumnIndex(5);
			if (recipe_.noise.thresholds.size() > 1) {
				ImVec2 button_size{ row_height, row_height };
				if (ImGui::Button("##RemoveNoiseRange", button_size)) {
					remove_region = i;
				}

				ImVec2 button_min{ ImGui::GetItemRectMin() };
				ImVec2 button_max{ ImGui::GetItemRectMax() };
				ImVec2 text_size{ ImGui::CalcTextSize("X") };
				ImVec2 text_position{
					button_min.x + (button_max.x - button_min.x - text_size.x) * 0.5f,
					button_min.y + (button_max.y - button_min.y - text_size.y) * 0.5f,
				};
				ImGui::GetWindowDrawList()->AddText(
					text_position, ImGui::GetColorU32(ImGuiCol_Text), "X"
				);
			}

			ImGui::PopID();
		}

		ImGui::EndTable();
	}

	if (remove_region >= 0 && recipe_.noise.thresholds.size() > 1) {
		int boundary{
			std::clamp(remove_region, 0, static_cast<int>(recipe_.noise.thresholds.size()) - 2)
		};

		recipe_.noise.thresholds[static_cast<std::size_t>(boundary)].maximum =
			recipe_.noise.thresholds[static_cast<std::size_t>(boundary + 1)].maximum;

		recipe_.noise.thresholds.erase(recipe_.noise.thresholds.begin() + boundary + 1);
	}

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
	auto it{ tile_slice_settings_.find(texture.value) };
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

	V2_int pixel_min{
		slice.x * settings->tile_size.x,
		slice.y * settings->tile_size.y,
	};
	V2_int pixel_max{ pixel_min + settings->tile_size };
	if (pixel_min.x < 0 || pixel_min.y < 0 || pixel_max.x > texture_size.x ||
		pixel_max.y > texture_size.y) {
		return {};
	}
	V2_float uv0{ static_cast<float>(pixel_min.x) / static_cast<float>(texture_size.x),
				  static_cast<float>(pixel_min.y) / static_cast<float>(texture_size.y) };
	V2_float uv1{ static_cast<float>(pixel_max.x) / static_cast<float>(texture_size.x),
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
	auto it{ std::ranges::find(tile_library_, id, &TileLibraryEntry::id) };
	return it == tile_library_.end() ? nullptr : &*it;
}

const PaintEditor::TileLibraryEntry* PaintEditor::FindTileEntry(std::string_view id) const {
	auto it{ std::ranges::find(tile_library_, id, &TileLibraryEntry::id) };
	return it == tile_library_.end() ? nullptr : &*it;
}

bool PaintEditor::DeleteTileLibraryEntry(EditorContext& ctx, std::string_view id) {
	const auto it{ std::ranges::find(tile_library_, id, &TileLibraryEntry::id) };
	if (it == tile_library_.end()) {
		return false;
	}

	std::size_t index{ static_cast<std::size_t>(it - tile_library_.begin()) };
	const TileLibraryEntry removed{ *it };
	const std::string selected_before{ selected_tile_entry_id_ };
	const std::optional<TextureKey> inspected_texture_before{ inspected_texture_ };
	V2_int inspected_slice_before{ inspected_slice_ };

	tile_library_.erase(it);
	if (selected_tile_entry_id_ == removed.id) {
		selected_tile_entry_id_.clear();
		inspected_texture_.reset();
	}
	SaveProjectLibrary(ctx);

	const std::string selected_after{ selected_tile_entry_id_ };
	const std::optional<TextureKey> inspected_texture_after{ inspected_texture_ };
	V2_int inspected_slice_after{ inspected_slice_ };
	PaintEditor* paint{ this };
	EditorContext* context{ &ctx };

	ctx.undo.PushApplied(
		"Remove Tile from Library",
		[paint, context, removed, index, selected_before, inspected_texture_before,
		 inspected_slice_before]() mutable {
			if (!paint->FindTileEntry(removed.id)) {
				std::size_t restore_index{ std::min(index, paint->tile_library_.size()) };
				auto position{ paint->tile_library_.begin() +
							   static_cast<std::ptrdiff_t>(restore_index) };
				paint->tile_library_.insert(position, removed);
			}
			paint->selected_tile_entry_id_ = selected_before;
			paint->inspected_texture_	   = inspected_texture_before;
			paint->inspected_slice_		   = inspected_slice_before;
			paint->SaveProjectLibrary(*context);
		},
		[paint, context, id = removed.id, selected_after, inspected_texture_after,
		 inspected_slice_after]() mutable {
			std::erase_if(paint->tile_library_, [&](const TileLibraryEntry& entry) {
				return entry.id == id;
			});
			paint->selected_tile_entry_id_ = selected_after;
			paint->inspected_texture_	   = inspected_texture_after;
			paint->inspected_slice_		   = inspected_slice_after;
			paint->SaveProjectLibrary(*context);
		}
	);

	return true;
}

PaintWeightedTileSet* PaintEditor::FindWeightedTileSet(std::string_view name) {
	auto it{ std::ranges::find(weighted_tile_sets_, name, &PaintWeightedTileSet::name) };
	return it == weighted_tile_sets_.end() ? nullptr : &*it;
}

const PaintWeightedTileSet* PaintEditor::FindWeightedTileSet(std::string_view name) const {
	auto it{ std::ranges::find(weighted_tile_sets_, name, &PaintWeightedTileSet::name) };
	return it == weighted_tile_sets_.end() ? nullptr : &*it;
}

PaintWeightedPrefabSet* PaintEditor::FindWeightedPrefabSet(std::string_view name) {
	auto it{ std::ranges::find(weighted_prefab_sets_, name, &PaintWeightedPrefabSet::name) };
	return it == weighted_prefab_sets_.end() ? nullptr : &*it;
}

const PaintWeightedPrefabSet* PaintEditor::FindWeightedPrefabSet(std::string_view name) const {
	auto it{ std::ranges::find(weighted_prefab_sets_, name, &PaintWeightedPrefabSet::name) };
	return it == weighted_prefab_sets_.end() ? nullptr : &*it;
}

PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(
	const TextureKey& texture, PaintAutotileFormat format
) {
	auto it{ std::ranges::find_if(autotile_rule_sets_, [&](const PaintAutotileRuleSet& rules) {
		return rules.texture == texture && rules.format == format;
	}) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

const PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(
	const TextureKey& texture, PaintAutotileFormat format
) const {
	auto it{ std::ranges::find_if(autotile_rule_sets_, [&](const PaintAutotileRuleSet& rules) {
		return rules.texture == texture && rules.format == format;
	}) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::uint64_t id) {
	auto it{ std::ranges::find(autotile_rule_sets_, id, &PaintAutotileRuleSet::id) };
	return it == autotile_rule_sets_.end() ? nullptr : &*it;
}

const PaintAutotileRuleSet* PaintEditor::FindAutotileRuleSet(std::uint64_t id) const {
	auto it{ std::ranges::find(autotile_rule_sets_, id, &PaintAutotileRuleSet::id) };
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

std::uint64_t PaintEditor::NextAutotileRuleSetId() const {
	std::uint64_t next{ 1 };
	for (const auto& rules : autotile_rule_sets_) {
		next = std::max(next, rules.id + 1);
	}
	return next;
}

bool PaintEditor::RefreshAutotileRuleSet(EditorContext& ctx, PaintAutotileRuleSet& rules) {
	if (!rules.texture) {
		rules.tiles.clear();
		return false;
	}

	const TileSliceSettings* slicing{ FindSliceSettings(rules.texture) };
	if (!slicing || !slicing->tile_size.IsPositive()) {
		rules.tiles.clear();
		return false;
	}

	V2_int texture_size{};
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
		return candidate.kind == AssetKind::Texture &&
			   candidate.key == static_cast<const AssetKey&>(rules.texture);
	}) };
	if (record != records.end() && record->metadata.dimensions.has_value()) {
		texture_size = *record->metadata.dimensions;
	}
	if (!texture_size.IsPositive() && ctx.editor.GetAssetManager().Has(rules.texture)) {
		texture_size = ctx.editor.GetAssetManager().GetTextureSize(rules.texture);
	}
	if (!texture_size.IsPositive() || texture_size.x % slicing->tile_size.x != 0 ||
		texture_size.y % slicing->tile_size.y != 0) {
		rules.tiles.clear();
		return false;
	}

	V2_int sheet_size{
		texture_size.x / slicing->tile_size.x,
		texture_size.y / slicing->tile_size.y,
	};
	auto layout{ ResolveAutotileAtlasLayout(rules.format, sheet_size) };
	if (!layout.has_value()) {
		rules.tiles.clear();
		return false;
	}

	rules.tiles.clear();
	rules.tiles.resize(static_cast<std::size_t>(layout->source_count));

	// The format defines the canonical block shape and slot order. A larger texture is
	// allowed; the top left complete block is used automatically and unrelated cells are ignored.
	for (int slot{}; slot < layout->source_count; ++slot) {
		PaintTileSource source{
			MakeTileSource(ctx, rules.texture, AutotileSourceSlice(*layout, slot))
		};
		if (!source) {
			rules.tiles.clear();
			return false;
		}
		rules.tiles[static_cast<std::size_t>(slot)] = std::move(source);
	}

	rules.name = rules.texture.value + " / " + AutotileFormatName(rules.format);
	return true;
}

PaintAutotileRuleSet* PaintEditor::EnsureAutotileRuleSet(
	EditorContext& ctx, const TextureKey& texture, PaintAutotileFormat format
) {
	if (!texture) {
		return nullptr;
	}

	if (auto* existing{ FindAutotileRuleSet(texture, format) }) {
		return RefreshAutotileRuleSet(ctx, *existing) ? existing : nullptr;
	}

	PaintAutotileRuleSet rules;
	rules.id	  = NextAutotileRuleSetId();
	rules.texture = texture;
	rules.format  = format;
	if (!RefreshAutotileRuleSet(ctx, rules)) {
		return nullptr;
	}

	autotile_rule_sets_.push_back(std::move(rules));
	SaveProjectLibrary(ctx);
	return &autotile_rule_sets_.back();
}

PaintAutotileRuleSet* PaintEditor::ResolveAutotileRuleSet(EditorContext& ctx) {
	if (!recipe_.autotile_texture) {
		return nullptr;
	}

	// A tileset's imported format is authoritative, not the last brush selection.
	auto imported{ std::ranges::find(
		autotile_library_, recipe_.autotile_texture, &AutotileLibraryEntry::texture
	) };
	if (imported == autotile_library_.end()) {
		return nullptr;
	}
	recipe_.autotile_format = imported->format;
	return EnsureAutotileRuleSet(ctx, imported->texture, imported->format);
}

void PaintEditor::RecomputeAutotileAround(
	Tilemap tilemap, V2_int cell, const PaintAutotileRuleSet&
) {
	RecomputeAutotileRegion(tilemap, { cell });
}

void PaintEditor::RecomputeAutotileRegion(
	Tilemap tilemap, const std::vector<V2_int>& changed_cells
) {
	if (!tilemap || changed_cells.empty()) {
		return;
	}

	auto& data{ tilemap.GetData() };
	std::unordered_map<V2_int, std::uint64_t> terrain;
	terrain.reserve(data.terrain.size() * 2 + 1);
	for (const TilemapTerrainCell& cell : data.terrain) {
		terrain.insert_or_assign(cell.coordinate, cell.ruleset_id);
	}

	std::unordered_map<std::uint64_t, const PaintAutotileRuleSet*> rules_by_id;
	for (const auto& rules : autotile_rule_sets_) {
		rules_by_id.insert_or_assign(rules.id, &rules);
	}
	const auto style_at = [&](V2_int cell) -> const PaintAutotileRuleSet* {
		auto it{ terrain.find(cell) };
		if (it == terrain.end()) {
			return nullptr;
		}
		auto rules{ rules_by_id.find(it->second) };
		return rules == rules_by_id.end() ? nullptr : rules->second;
	};
	const auto corner_style = [](const PaintAutotileRuleSet* rules) {
		return rules && (rules->format == PaintAutotileFormat::DualGrid16 ||
						 rules->format == PaintAutotileFormat::Classic15);
	};

	std::unordered_set<V2_int> changed;
	std::unordered_set<V2_int> affected;
	changed.reserve(changed_cells.size() * 2);
	affected.reserve(changed_cells.size() * 2 + 16);
	for (V2_int cell : changed_cells) {
		changed.insert(cell);
		for (int y{ -1 }; y <= 1; ++y) {
			for (int x{ -1 }; x <= 1; ++x) {
				affected.insert(cell + V2_int{ x, y });
			}
		}
	}

	// Existing tiles are indexed only once. Neither terrain lookup nor display
	// updates scan the growing vector, even when filling tens of thousands of cells.
	std::unordered_set<V2_int> existing_terrain_tiles;
	existing_terrain_tiles.reserve(data.tiles.size());
	for (const TilemapTile& tile : data.tiles) {
		if (tile.terrain_ruleset_id) {
			existing_terrain_tiles.insert(tile.coordinate);
		}
	}

	std::unordered_map<V2_int, TilemapTile> updated;
	std::unordered_set<V2_int> erased;
	updated.reserve(affected.size());
	erased.reserve(affected.size() / 4 + 1);

	static constexpr std::array<V2_int, 4> corners{
		V2_int{ 0, 0 },
		V2_int{ 1, 0 },
		V2_int{ 0, 1 },
		V2_int{ 1, 1 },
	};
	static constexpr std::array<V2_int, 8> neighbors{
		V2_int{ 0, -1 }, V2_int{ 1, -1 }, V2_int{ 1, 0 },  V2_int{ 1, 1 },
		V2_int{ 0, 1 },	 V2_int{ -1, 1 }, V2_int{ -1, 0 }, V2_int{ -1, -1 },
	};

	for (V2_int cell : affected) {
		const PaintAutotileRuleSet* style{ style_at(cell) };
		if (!style || corner_style(style) || !changed.contains(cell)) {
			// Favor terrain just painted at this corner. A normal grid tile
			// painted at the same display cell keeps priority over its neighbors.
			bool picked_changed_corner{};
			for (V2_int delta : corners) {
				V2_int logical{ cell + delta };
				const auto* candidate{ style_at(logical) };
				if (corner_style(candidate) && changed.contains(logical)) {
					style				  = candidate;
					picked_changed_corner = true;
					break;
				}
			}
			if (!picked_changed_corner && !corner_style(style)) {
				for (V2_int delta : corners) {
					const auto* candidate{ style_at(cell + delta) };
					if (corner_style(candidate)) {
						style = candidate;
						break;
					}
				}
			}
		}

		if (!style) {
			if (changed.contains(cell) || existing_terrain_tiles.contains(cell)) {
				erased.insert(cell);
			}
			continue;
		}

		int mask{};
		bool interior{ changed.contains(cell) };
		if (corner_style(style)) {
			for (V2_int delta : corners) {
				V2_int logical{ cell + delta };
				auto it{ terrain.find(logical) };
				if (it == terrain.end() || it->second != style->id || !changed.contains(logical)) {
					interior = false;
					break;
				}
			}
			if (interior) {
				mask = 15;
			} else {
				for (std::size_t i{}; i < corners.size(); ++i) {
					V2_int logical{ cell + corners[i] };
					auto it{ terrain.find(logical) };
					if (it != terrain.end() && it->second == style->id) {
						mask |= 1 << i;
					}
				}
			}
		} else {
			if (style_at(cell) != style) {
				if (changed.contains(cell) || existing_terrain_tiles.contains(cell)) {
					erased.insert(cell);
				}
				continue;
			}
			// Interior cells have a fixed 4 or 8 neighbor mask. Only edges
			// require checking the individual neighbor configuration.
			for (V2_int delta : neighbors) {
				V2_int neighbor{ cell + delta };
				if (!changed.contains(neighbor) || style_at(neighbor) != style) {
					interior = false;
					break;
				}
			}
			if (interior) {
				mask = style->format == PaintAutotileFormat::Blob47 ? 46 : 15;
			} else {
				auto occupied = [&](V2_int coordinate) {
					auto it{ terrain.find(coordinate) };
					return it != terrain.end() && it->second == style->id;
				};
				mask = AutotileIndex(style->format, occupied, cell);
			}
		}

		int index{ corner_style(style) && style->tiles.size() == 15 ? mask - 1 : mask };
		if (index < 0 || index >= static_cast<int>(style->tiles.size()) ||
			!style->tiles[static_cast<std::size_t>(index)]) {
			if (changed.contains(cell) || existing_terrain_tiles.contains(cell)) {
				erased.insert(cell);
			}
			continue;
		}
		const PaintTileSource& source{ *style->tiles[static_cast<std::size_t>(index)] };
		updated.insert_or_assign(cell, TilemapTile{
			.coordinate = cell,
			.texture = source.texture,
			.texture_coordinates = source.texture_coordinates,
			.pixel_size = {
				std::max(1, static_cast<int>(std::lround(data.cell_size.x))),
				std::max(1, static_cast<int>(std::lround(data.cell_size.y))),
			},
			.origin = Origin::TopLeft,
			.offset = style->format == PaintAutotileFormat::DualGrid16
				? data.cell_size * 0.5f : V2_float{},
			.tint = color::White,
			.terrain_ruleset_id = style->id,
		});
	}

	std::vector<TilemapTile> next_tiles;
	next_tiles.reserve(data.tiles.size() + updated.size());
	for (TilemapTile& tile : data.tiles) {
		if (auto it{ updated.find(tile.coordinate) }; it != updated.end()) {
			next_tiles.push_back(std::move(it->second));
			updated.erase(it);
		} else if (!erased.contains(tile.coordinate)) {
			next_tiles.push_back(std::move(tile));
		}
	}
	for (auto& [coordinate, tile] : updated) {
		next_tiles.push_back(std::move(tile));
	}
	data.tiles = std::move(next_tiles);
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
		PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
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
		bool group_match{ ContainsInsensitive(group, tile_source_search_) };
		bool any_match{ std::ranges::any_of(tile_library_, [&](const TileLibraryEntry& entry) {
			return entry.group == group &&
				   (group_match || ContainsInsensitive(entry.name, tile_source_search_) ||
					ContainsInsensitive(entry.texture.value, tile_source_search_));
		}) };
		if (!any_match) {
			continue;
		}

		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen };

		ImGui::PushID(group.c_str());
		bool open{ ImGui::TreeNodeEx("##PaintTileSourceGroup", flags, "%s", group.c_str()) };

		if (open) {
			constexpr float image_side{ 40.0f };
			float row_right{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
			bool first_on_row{ true };

			for (const auto& entry : tile_library_) {
				if (entry.group != group) {
					continue;
				}
				if (!group_match && !ContainsInsensitive(entry.name, tile_source_search_) &&
					!ContainsInsensitive(entry.texture.value, tile_source_search_)) {
					continue;
				}

				PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
				if (!candidate) {
					continue;
				}

				if (!first_on_row) {
					float next_right{ ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x +
									  image_side };
					if (next_right <= row_right) {
						ImGui::SameLine();
					}
				}

				ImGui::PushID(entry.id.c_str());

				ImVec2 p0{ ImGui::GetCursorScreenPos() };
				ImGui::InvisibleButton("##PaintTileSource", { image_side, image_side });

				bool selected{ source && source == candidate };
				bool hovered{ ImGui::IsItemHovered() };
				auto* draw{ ImGui::GetWindowDrawList() };
				auto record{ record_for(entry.texture) };

				if (record != records.end() && record->preview.has_value()) {
					const auto& uv{ candidate.texture_coordinates };
					ImVec2 p1{ p0.x + image_side, p0.y + image_side };

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
		if (auto it{ std::ranges::find_if(
				tile_library_,
				[&](const TileLibraryEntry& entry) {
					PaintTileSource candidate{ MakeTileSource(ctx, entry.texture, entry.slice) };
					return candidate && candidate == *source;
				}
			) };
			it != tile_library_.end()) {
			preview = it->name;
		} else {
			preview = source->texture.value;
		}
	}

	float requested_width{ ImGui::CalcItemWidth() };
	float thumbnail_side{ ImGui::GetFrameHeight() };
	float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };

	if (source.has_value() && *source) {
		DrawTileThumbnail(ctx, *source, thumbnail_side);
	} else {
		ImVec2 p0{ ImGui::GetCursorScreenPos() };
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
		bool group_match{ ContainsInsensitive(group, tile_source_search_) };
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
		float row_right{ ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x };
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
				float next_right{ ImGui::GetItemRectMax().x + ImGui::GetStyle().ItemSpacing.x +
								  image_side };
				if (next_right <= row_right) {
					ImGui::SameLine();
				}
			}

			ImGui::PushID(entry.id.c_str());
			ImVec2 p0{ ImGui::GetCursorScreenPos() };
			ImGui::InvisibleButton("##TileSourceThumb", { image_side, image_side });

			bool selected{ source.has_value() && *source == candidate };
			bool hovered{ ImGui::IsItemHovered() };
			auto* draw{ ImGui::GetWindowDrawList() };

			auto record{ std::ranges::find_if(records, [&](const auto& asset) {
				return asset.kind == AssetKind::Texture &&
					   asset.key == static_cast<const AssetKey&>(entry.texture);
			}) };

			if (record != records.end() && record->preview.has_value()) {
				const auto& uv{ candidate.texture_coordinates };
				ImVec2 p1{ p0.x + image_side, p0.y + image_side };

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
		std::string group{ GetPrefabGroup(ctx, key) };
		if (ContainsInsensitive(PrefabDisplayName(key), prefab_source_search_) ||
			ContainsInsensitive(group, prefab_source_search_)) {
			groups.push_back(group);
		}
	}
	SortGroupsUngroupedFirst(groups);
	for (const std::string& group : groups) {
		ImGuiTreeNodeFlags flags{ ImGuiTreeNodeFlags_SpanAvailWidth |
								  ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_DefaultOpen };
		ImGui::PushID(group.c_str());
		if (ImGui::TreeNodeEx("##PaintPrefabSourceGroup", flags, "%s", group.c_str())) {
			for (const PrefabKey& key : keys) {
				if (GetPrefabGroup(ctx, key) != group) {
					continue;
				}
				std::string name{ PrefabDisplayName(key) };
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
	std::string preview{ source.has_value() ? PrefabDisplayName(*source)
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
		std::string group{ GetPrefabGroup(ctx, key) };
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
			std::string name{ PrefabDisplayName(key) };
			if (!ContainsInsensitive(name, prefab_source_search_) &&
				!ContainsInsensitive(group, prefab_source_search_)) {
				continue;
			}
			bool selected{ source.has_value() && *source == key };
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

	float source_popup_inset{ ComboPopupSideInset() };
	PaintWeightedTileSet* active{ FindWeightedTileSet(recipe_.weighted_tile_set_name) };

	BeginRecipeFillRow("Weighted Set");
	float add_width{ ImGui::CalcTextSize("Add Weighted Set").x +
					 ImGui::GetStyle().FramePadding.x * 2.0f };
	float delete_width{ ImGui::CalcTextSize("Delete Set").x +
						ImGui::GetStyle().FramePadding.x * 2.0f };
	float row_spacing{ ImGui::GetStyle().ItemSpacing.x };
	const char* preview{ active ? active->name.c_str() : "<none>" };
	float minimum_combo_width{ ImGui::CalcTextSize(preview).x +
							   ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight() };
	ImGui::SetNextItemWidth(
		std::max(
			minimum_combo_width, AvailableContentWidth(source_popup_inset) - add_width -
									 delete_width - row_spacing * 2.0f
		)
	);
	if (ImGui::BeginCombo("##PaintRecipeWeightedTileSet", preview)) {
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
		std::string name{ active->name };
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
		return;
	}

	std::string name{ active->name };
	BeginRecipeFillField("Set Name", source_popup_inset);
	if (ImGui::InputText("##WeightedTileSetName", &name) && !name.empty() && name != active->name &&
		!FindWeightedTileSet(name)) {
		std::string old{ active->name };
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
				ImGuiTableFlags_SizingStretchProp,
			ImVec2{ AvailableContentWidth(source_popup_inset), 0.0f }
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
				if (auto it{ std::ranges::find_if(
						tile_library_,
						[&](const TileLibraryEntry& candidate) {
							PaintTileSource resolved{
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
			bool group_match{ ContainsInsensitive(group, tile_source_search_) };
			if (!ImGui::TreeNodeEx(
					group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
				)) {
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

				bool exists{ std::ranges::any_of(
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

	float source_popup_inset{ ComboPopupSideInset() };
	PaintWeightedPrefabSet* active{ FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) };

	BeginRecipeFillRow("Weighted Set");
	float add_width{ ImGui::CalcTextSize("Add Weighted Set").x +
					 ImGui::GetStyle().FramePadding.x * 2.0f };
	float delete_width{ ImGui::CalcTextSize("Delete Set").x +
						ImGui::GetStyle().FramePadding.x * 2.0f };
	float row_spacing{ ImGui::GetStyle().ItemSpacing.x };
	const char* preview{ active ? active->name.c_str() : "<none>" };
	float minimum_combo_width{ ImGui::CalcTextSize(preview).x +
							   ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetFrameHeight() };
	ImGui::SetNextItemWidth(
		std::max(
			minimum_combo_width, AvailableContentWidth(source_popup_inset) - add_width -
									 delete_width - row_spacing * 2.0f
		)
	);
	if (ImGui::BeginCombo("##PaintRecipeWeightedPrefabSet", preview)) {
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
		std::string name{ active->name };
		std::erase_if(weighted_prefab_sets_, [&](const auto& set) { return set.name == name; });
		recipe_.weighted_prefab_set_name =
			weighted_prefab_sets_.empty() ? std::string{} : weighted_prefab_sets_.front().name;
		SaveProjectLibrary(ctx);
		active = FindWeightedPrefabSet(recipe_.weighted_prefab_set_name);
	}
	ImGui::EndDisabled();

	if (!active) {
		return;
	}

	std::string name{ active->name };
	BeginRecipeFillField("Set Name", source_popup_inset);
	if (ImGui::InputText("##WeightedPrefabSetName", &name) && !name.empty() &&
		name != active->name && !FindWeightedPrefabSet(name)) {
		std::string old{ active->name };
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
				ImGuiTableFlags_SizingStretchProp,
			ImVec2{ AvailableContentWidth(source_popup_inset), 0.0f }
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
			std::string group{ GetPrefabGroup(ctx, key) };
			if (ContainsInsensitive(PrefabDisplayName(key), prefab_source_search_) ||
				ContainsInsensitive(group, prefab_source_search_)) {
				groups.push_back(group);
			}
		}
		SortGroupsUngroupedFirst(groups);

		for (const auto& group : groups) {
			if (!ImGui::TreeNodeEx(
					group.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_OpenOnArrow
				)) {
				continue;
			}

			for (const auto& key : keys) {
				if (GetPrefabGroup(ctx, key) != group) {
					continue;
				}

				std::string display{ PrefabDisplayName(key) };
				if (!ContainsInsensitive(display, prefab_source_search_) &&
					!ContainsInsensitive(group, prefab_source_search_)) {
					continue;
				}

				bool exists{ std::ranges::any_of(active->entries, [&](const auto& entry) {
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

void PaintEditor::DrawAutotileSourceEditor(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);

	float source_popup_inset{ ComboPopupSideInset() };
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	auto record_for = [&](const TextureKey& texture) {
		return std::ranges::find_if(records, [&](const auto& record) {
			return record.kind == AssetKind::Texture &&
				   record.key == static_cast<const AssetKey&>(texture);
		});
	};

	auto library_for = [&](const TextureKey& texture) {
		return std::ranges::find(autotile_library_, texture, &AutotileLibraryEntry::texture);
	};

	// Imported autotiles own their format. The brush only selects the tileset.
	auto selected_entry{ library_for(recipe_.autotile_texture) };
	if (selected_entry != autotile_library_.end()) {
		recipe_.autotile_format = selected_entry->format;
	}

	auto draw_sheet_thumbnail = [&](const TextureKey& texture, float side) {
		auto record{ record_for(texture) };
		ImVec2 p0{ ImGui::GetCursorScreenPos() };
		ImGui::Dummy({ side, side });

		auto* draw{ ImGui::GetWindowDrawList() };
		draw->AddRectFilled(
			p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_FrameBg),
			ImGui::GetStyle().FrameRounding
		);

		if (record != records.end() && record->preview.has_value()) {
			V2_int image_size{ record->preview->size };
			if (!image_size.IsPositive()) {
				image_size = { 1, 1 };
			}
			float scale{ std::min(
				side / static_cast<float>(std::max(1, image_size.x)),
				side / static_cast<float>(std::max(1, image_size.y))
			) };
			ImVec2 image_extent{
				std::max(1.0f, static_cast<float>(image_size.x) * scale),
				std::max(1.0f, static_cast<float>(image_size.y) * scale),
			};
			ImVec2 image_min{
				p0.x + (side - image_extent.x) * 0.5f,
				p0.y + (side - image_extent.y) * 0.5f,
			};
			ImVec2 image_max{
				image_min.x + image_extent.x,
				image_min.y + image_extent.y,
			};
			draw->AddImage(
				static_cast<ImTextureID>(record->preview->texture), image_min, image_max
			);
		}

		draw->AddRect(
			p0, { p0.x + side, p0.y + side }, ImGui::GetColorU32(ImGuiCol_Border),
			ImGui::GetStyle().FrameRounding
		);
	};

	BeginRecipeFillRow("Tileset");

	float requested_width{ AvailableContentWidth(source_popup_inset) };
	float thumbnail_side{ ImGui::GetFrameHeight() };
	float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };

	if (recipe_.autotile_texture) {
		draw_sheet_thumbnail(recipe_.autotile_texture, thumbnail_side);
	} else {
		ImVec2 p0{ ImGui::GetCursorScreenPos() };
		ImGui::Dummy({ thumbnail_side, thumbnail_side });
		auto* draw{ ImGui::GetWindowDrawList() };
		draw->AddRectFilled(
			p0, { p0.x + thumbnail_side, p0.y + thumbnail_side },
			ImGui::GetColorU32(ImGuiCol_FrameBg), ImGui::GetStyle().FrameRounding
		);
		draw->AddRect(
			p0, { p0.x + thumbnail_side, p0.y + thumbnail_side },
			ImGui::GetColorU32(ImGuiCol_Border), ImGui::GetStyle().FrameRounding
		);
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::SetNextItemWidth(std::max(1.0f, requested_width - thumbnail_side - spacing));

	std::string preview{ "<none>" };
	if (selected_entry != autotile_library_.end()) {
		preview = selected_entry->name.empty() ? TextureDisplayName(selected_entry->texture)
											   : selected_entry->name;
	} else if (recipe_.autotile_texture) {
		preview = TextureDisplayName(recipe_.autotile_texture);
	}

	if (ImGui::BeginCombo("##PaintAutotileTileset", preview.c_str())) {
		if (ImGui::Selectable("<none>", !recipe_.autotile_texture)) {
			recipe_.autotile_texture = {};
		}

		// Use the saved paint library as the authority. Asset record iteration alone
		// excluded imported tilesets when their metadata or load state was unavailable.
		for (const auto& entry : autotile_library_) {
			ImGui::PushID(entry.texture.value.c_str());
			bool selected{ recipe_.autotile_texture == entry.texture };
			std::string label{ entry.name.empty() ? TextureDisplayName(entry.texture)
												  : entry.name };
			label += " (";
			label += AutotileFormatName(entry.format);
			label += ")";
			if (ImGui::Selectable(label.c_str(), selected)) {
				recipe_.autotile_texture = entry.texture;
				recipe_.autotile_format	 = entry.format;
				ResolveAutotileRuleSet(ctx);
			}
			if (selected) {
				ImGui::SetItemDefaultFocus();
			}
			ImGui::PopID();
		}

		ImGui::EndCombo();
	}

	if (selected_entry == autotile_library_.end()) {
		ImGui::TextDisabled("Import an autotile in the Tiles window to assign its layout.");
		return;
	}

	BeginRecipeFillRow("Layout");
	ImGui::TextDisabled("%s", AutotileFormatName(selected_entry->format));

	const TileSliceSettings* slicing{ FindSliceSettings(recipe_.autotile_texture) };
	if (!slicing || !slicing->tile_size.IsPositive()) {
		ImGui::TextDisabled("This tileset is missing its imported tile size.");
		return;
	}

	ResolveAutotileRuleSet(ctx);
	ImGui::TextDisabled(
		"%dx%d tiles. Layout saved with the imported tileset.", slicing->tile_size.x,
		slicing->tile_size.y
	);
}

void PaintEditor::EnsureLocalState(EditorContext& ctx) {
	auto root{ ctx.editor.GetProjectRoot() };
	std::string project{ root.has_value() ? root->lexically_normal().generic_string()
										  : std::string{} };
	if (local_state_loaded_ && project == loaded_local_project_) {
		return;
	}

	local_state_loaded_	  = true;
	loaded_local_project_ = project;
	PaintLocalState state{ ctx.local.paint.value_or(PaintLocalState{}) };
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

void PaintEditor::RegisterAutotileTileset(
	TextureKey texture, PaintAutotileFormat format, V2_int tile_size, std::string group
) {
	if (!texture || !tile_size.IsPositive()) {
		return;
	}
	auto it{
		std::ranges::find(registered_autotiles_, texture, &RegisteredAutotileTileset::texture)
	};
	RegisteredAutotileTileset definition{
		.texture   = std::move(texture),
		.format	   = format,
		.tile_size = tile_size,
		.group	   = std::move(group),
	};
	if (it == registered_autotiles_.end()) {
		registered_autotiles_.push_back(std::move(definition));
	} else {
		*it = std::move(definition);
	}
}

void PaintEditor::EnsureProjectLibrary(EditorContext& ctx) {
	auto root{ ctx.editor.GetProjectRoot() };
	std::string project{ root.has_value() ? root->lexically_normal().generic_string()
										  : std::string{} };
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::vector<std::string> texture_keys;
	for (const auto& record : records) {
		if (record.kind == AssetKind::Texture && !record.engine_asset) {
			texture_keys.push_back(record.key.value);
		}
	}
	std::ranges::sort(texture_keys);
	if (project_library_loaded_ && project == loaded_project_library_) {
		if (loaded_texture_keys_ != texture_keys) {
			loaded_texture_keys_ = std::move(texture_keys);
			ReconcileProjectLibrary(ctx);
		}
		return;
	}

	loaded_texture_keys_	= std::move(texture_keys);
	project_library_loaded_ = true;
	loaded_project_library_ = project;
	tile_slice_settings_.clear();
	tile_library_.clear();
	autotile_library_.clear();
	tile_groups_.clear();
	autotile_groups_.clear();
	prefab_groups_.clear();
	prefab_group_by_key_.clear();
	weighted_tile_sets_.clear();
	weighted_prefab_sets_.clear();
	autotile_rule_sets_.clear();

	const PaintProjectState& state{ ctx.project_state.paint };
	tile_groups_		  = state.tile_groups;
	autotile_groups_	  = state.autotile_groups.value_or(std::vector<std::string>{});
	prefab_groups_		  = state.prefab_groups;
	weighted_tile_sets_	  = state.weighted_tile_sets;
	weighted_prefab_sets_ = state.weighted_prefab_sets;
	autotile_rule_sets_	  = state.autotile_rule_sets;
	for (const auto& stored : state.tile_slice_settings) {
		tile_slice_settings_.insert_or_assign(
			stored.texture.value, TileSliceSettings{
									  .tile_size = stored.tile_size,
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
	for (const auto& stored :
		 state.autotiles.value_or(std::vector<PaintAutotileLibraryEntryState>{})) {
		autotile_library_.push_back(
			AutotileLibraryEntry{
				.name		 = stored.name,
				.texture	 = stored.texture,
				.format		 = stored.format,
				.group		 = stored.group,
				.show_slices = stored.show_slices.value_or(false),
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
	state.tile_groups	  = tile_groups_;
	state.autotile_groups = autotile_groups_;
	state.prefab_groups	  = prefab_groups_;
	state.tile_slice_settings.reserve(tile_slice_settings_.size());
	for (const auto& [texture, settings] : tile_slice_settings_) {
		state.tile_slice_settings.push_back(
			PaintTileSliceSettingsState{
				.texture   = TextureKey{ texture },
				.tile_size = settings.tile_size,
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

	state.autotiles.emplace();
	state.autotiles->reserve(autotile_library_.size());
	for (const auto& entry : autotile_library_) {
		state.autotiles->push_back(
			PaintAutotileLibraryEntryState{
				.name		 = entry.name,
				.texture	 = entry.texture,
				.format		 = entry.format,
				.group		 = entry.group,
				.show_slices = entry.show_slices,
			}
		);
	}

	state.weighted_tile_sets	 = weighted_tile_sets_;
	state.weighted_prefab_sets	 = weighted_prefab_sets_;
	state.autotile_rule_sets	 = autotile_rule_sets_;
	state.entity_exclusion_masks = ctx.project_state.paint.entity_exclusion_masks;

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
	bool project_library_changed{};
	for (const auto& record : records) {
		if (record.kind != AssetKind::Texture || record.engine_asset) {
			continue;
		}

		texture_keys.insert(record.key.value);
		TextureKey texture{ record.key };

		// The catalog contains textures that have never been added to the Tiles window.
		// Do not add those textures or create palette slicing metadata for them.
		auto declaration{
			std::ranges::find(registered_autotiles_, texture, &RegisteredAutotileTileset::texture)
		};
		bool in_tile_library{ std::ranges::any_of(
			tile_library_, [&](const TileLibraryEntry& entry) { return entry.texture == texture; }
		) };
		bool in_autotile_library{ std::ranges::any_of(
			autotile_library_,
			[&](const AutotileLibraryEntry& entry) { return entry.texture == texture; }
		) };

		if (declaration == registered_autotiles_.end() && !in_tile_library &&
			!in_autotile_library) {
			continue;
		}

		V2_int texture_size{};

		if (record.metadata.dimensions.has_value()) {
			texture_size = *record.metadata.dimensions;
		}

		if (!texture_size.IsPositive() && ctx.editor.GetAssetManager().Has(texture)) {
			texture_size = ctx.editor.GetAssetManager().GetTextureSize(texture);
		}

		auto [settings_it, inserted]{ tile_slice_settings_.try_emplace(texture.value) };

		if (inserted && texture_size.IsPositive()) {
			settings_it->second.tile_size = texture_size;
		}

		// An explicit autotile registration is an authoring action, not catalog discovery.
		if (declaration != registered_autotiles_.end() && !in_autotile_library) {
			settings_it->second.tile_size = declaration->tile_size;
			std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
				return entry.texture == texture;
			});
			autotile_library_.push_back(
				AutotileLibraryEntry{
					.name	 = TextureDisplayName(texture),
					.texture = texture,
					.format	 = declaration->format,
					.group	 = declaration->group,
				}
			);
			project_library_changed = true;
		}
	}
	std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
		return !texture_keys.contains(entry.texture.value);
	});
	auto autotile_count{ autotile_library_.size() };
	std::erase_if(autotile_library_, [&](const AutotileLibraryEntry& entry) {
		return !texture_keys.contains(entry.texture.value);
	});
	project_library_changed |= autotile_count != autotile_library_.size();

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
	auto autotile_rule_count{ autotile_rule_sets_.size() };
	std::erase_if(autotile_rule_sets_, [](const PaintAutotileRuleSet& rules) {
		return !rules.texture;
	});
	project_library_changed |= autotile_rule_sets_.size() != autotile_rule_count;

	std::unordered_set<std::uint64_t> autotile_ids;
	std::uint64_t next_autotile_id{ 1 };
	for (const auto& rules : autotile_rule_sets_) {
		next_autotile_id = std::max(next_autotile_id, rules.id + 1);
	}
	for (auto& rules : autotile_rule_sets_) {
		if (rules.id == 0 || !autotile_ids.insert(rules.id).second) {
			rules.id = next_autotile_id++;
			autotile_ids.insert(rules.id);
			project_library_changed = true;
		}

		if (!texture_keys.contains(rules.texture.value)) {
			rules.tiles.clear();
			continue;
		}

		auto before{ rules.tiles };
		RefreshAutotileRuleSet(ctx, rules);
		project_library_changed |= before != rules.tiles;
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
	if (recipe_.autotile_texture && !texture_keys.contains(recipe_.autotile_texture.value)) {
		recipe_.autotile_texture = {};
	}

	project_library_changed |= NormalizeAutotileGroups();

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

	if (project_library_changed) {
		SaveProjectLibrary(ctx);
	}
}

bool PaintEditor::IsAutotileGroup(std::string_view group) const {
	return group != "Ungrouped" && std::ranges::contains(autotile_groups_, group);
}

bool PaintEditor::NormalizeAutotileGroups() {
	// An autotile always has an exclusive group. Existing projects that stored
	// several autotiles in one group are separated during reconciliation.
	bool changed{};
	std::vector<std::string> groups;
	std::unordered_set<std::string> assigned;
	for (const auto& name : autotile_groups_) {
		if (!name.empty() && name != "Ungrouped" && !std::ranges::contains(groups, name)) {
			groups.push_back(name);
		} else {
			changed = true;
		}
	}

	auto normal_group_in_use = [&](std::string_view name) {
		return std::ranges::any_of(tile_library_, [&](const TileLibraryEntry& tile) {
			return tile.group == name;
		});
	};
	auto unique_name = [&](std::string name) {
		if (name.empty() || name == "Ungrouped") {
			name = "Autotile";
		}
		std::string candidate{ name };
		int number{ 2 };
		while (assigned.contains(candidate) || normal_group_in_use(candidate) ||
			   (std::ranges::contains(tile_groups_, candidate) &&
				!std::ranges::contains(groups, candidate))) {
			candidate = name + " " + std::to_string(number++);
		}
		return candidate;
	};

	for (auto& entry : autotile_library_) {
		std::string original{ entry.group };
		if (original.empty() || original == "Ungrouped" || assigned.contains(original) ||
			(normal_group_in_use(original) && !std::ranges::contains(groups, original)) ||
			(std::ranges::contains(tile_groups_, original) &&
			 !std::ranges::contains(groups, original))) {
			entry.group = unique_name(entry.name);
			changed		= true;
		}
		assigned.insert(entry.group);
		if (!std::ranges::contains(groups, entry.group)) {
			groups.push_back(entry.group);
			changed = true;
		}
	}

	// A group that was already marked exclusive in an older project cannot
	// contain manually imported tiles, even when the atlas is shown as slices.
	for (auto& tile : tile_library_) {
		if (std::ranges::contains(groups, tile.group)) {
			tile.group = "Ungrouped";
			changed	   = true;
		}
	}
	std::ranges::sort(groups);
	auto old_normal_count{ tile_groups_.size() };
	std::erase_if(tile_groups_, [&](const std::string& name) {
		return std::ranges::contains(groups, name) && !normal_group_in_use(name);
	});
	changed |= old_normal_count != tile_groups_.size();
	if (groups != autotile_groups_) {
		autotile_groups_ = std::move(groups);
		changed			 = true;
	}
	return changed;
}

std::vector<std::string> PaintEditor::TileGroupNames() const {
	std::vector<std::string> groups{ "Ungrouped" };
	groups.insert(groups.end(), tile_groups_.begin(), tile_groups_.end());
	for (const auto& group : autotile_groups_) {
		if (!std::ranges::contains(groups, group)) {
			groups.push_back(group);
		}
	}
	for (const auto& entry : tile_library_) {
		if (entry.group != "Ungrouped" && !std::ranges::contains(groups, entry.group)) {
			groups.emplace_back(entry.group);
		}
	}
	for (const auto& entry : autotile_library_) {
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
	auto it{ prefab_group_by_key_.find(key.value) };
	return it == prefab_group_by_key_.end() || it->second.empty() ? "Ungrouped" : it->second;
}

bool PaintEditor::CreatePrefabGroup(EditorContext& ctx, std::string name) {
	EnsureProjectLibrary(ctx);
	name = Trim(std::move(name));
	if (name.empty() || name == "Ungrouped" || std::ranges::contains(GetPrefabGroups(ctx), name)) {
		return false;
	}
	GroupSnapshot before{ prefab_groups_, prefab_group_by_key_ };
	prefab_groups_.emplace_back(std::move(name));
	std::ranges::sort(prefab_groups_);
	SaveProjectLibrary(ctx);
	RecordGroupChange(
		ctx, "Create Prefab Group", std::move(before),
		GroupSnapshot{ prefab_groups_, prefab_group_by_key_ },
		[this](EditorContext& context, const GroupSnapshot& state) {
			prefab_groups_		 = state.groups;
			prefab_group_by_key_ = state.members;
			SaveProjectLibrary(context);
		}
	);
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
	if (new_name != old_name && std::ranges::contains(GetPrefabGroups(ctx), new_name)) {
		return false;
	}
	if (old_name == new_name ||
		!std::ranges::contains(GetPrefabGroups(ctx), std::string{ old_name })) {
		return false;
	}
	GroupSnapshot before{ prefab_groups_, prefab_group_by_key_ };
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
	RecordGroupChange(
		ctx, "Rename Prefab Group", std::move(before),
		GroupSnapshot{ prefab_groups_, prefab_group_by_key_ },
		[this](EditorContext& context, const GroupSnapshot& state) {
			prefab_groups_		 = state.groups;
			prefab_group_by_key_ = state.members;
			SaveProjectLibrary(context);
		}
	);
	return true;
}

bool PaintEditor::DeletePrefabGroup(EditorContext& ctx, std::string_view name) {
	EnsureProjectLibrary(ctx);
	if (name == "Ungrouped" || !std::ranges::contains(GetPrefabGroups(ctx), std::string{ name })) {
		return false;
	}
	GroupSnapshot before{ prefab_groups_, prefab_group_by_key_ };
	std::erase_if(prefab_groups_, [&](const std::string& group) { return group == name; });
	for (auto& [_, group] : prefab_group_by_key_) {
		if (group == name) {
			group = "Ungrouped";
		}
	}
	SaveProjectLibrary(ctx);
	RecordGroupChange(
		ctx, "Delete Prefab Group", std::move(before),
		GroupSnapshot{ prefab_groups_, prefab_group_by_key_ },
		[this](EditorContext& context, const GroupSnapshot& state) {
			prefab_groups_		 = state.groups;
			prefab_group_by_key_ = state.members;
			SaveProjectLibrary(context);
		}
	);
	return true;
}

void PaintEditor::MovePrefabToGroup(EditorContext& ctx, const PrefabKey& key, std::string group) {
	EnsureProjectLibrary(ctx);
	if (group.empty()) {
		group = "Ungrouped";
	}
	if (GetPrefabGroup(ctx, key) == group) {
		return;
	}
	GroupSnapshot before{ prefab_groups_, prefab_group_by_key_ };
	if (group == "Ungrouped") {
		prefab_group_by_key_.erase(key.value);
	} else {
		prefab_group_by_key_.insert_or_assign(key.value, std::move(group));
	}
	SaveProjectLibrary(ctx);
	RecordGroupChange(
		ctx, "Move Prefab to Group", std::move(before),
		GroupSnapshot{ prefab_groups_, prefab_group_by_key_ },
		[this](EditorContext& context, const GroupSnapshot& state) {
			prefab_groups_		 = state.groups;
			prefab_group_by_key_ = state.members;
			SaveProjectLibrary(context);
		}
	);
}

void PaintEditor::OnPrefabRenamed(
	EditorContext& ctx, const PrefabKey& old_key, const PrefabKey& new_key
) {
	EnsureProjectLibrary(ctx);

	// Group membership, prefab references and weighted sets must all be restored
	// together with the asset rename by the parent undo command.
	auto capture = [this]() {
		return std::tuple{ prefab_groups_, prefab_group_by_key_, prefab_source_, recipe_,
						   weighted_prefab_sets_ };
	};
	auto before{ capture() };
	bool changed{ false };

	if (auto it{ prefab_group_by_key_.find(old_key.value) }; it != prefab_group_by_key_.end()) {
		prefab_group_by_key_[new_key.value] = it->second;
		prefab_group_by_key_.erase(it);
		changed = true;
	}
	if (prefab_source_ == old_key) {
		prefab_source_ = new_key;
		changed		   = true;
	}
	if (recipe_.checker_prefab == old_key) {
		recipe_.checker_prefab = new_key;
		changed				   = true;
	}
	for (auto& threshold : recipe_.noise.thresholds) {
		if (threshold.prefab == old_key) {
			threshold.prefab = new_key;
			changed			 = true;
		}
	}
	for (auto& set : weighted_prefab_sets_) {
		for (auto& entry : set.entries) {
			if (entry.prefab == old_key) {
				entry.prefab = new_key;
				changed		 = true;
			}
		}
	}
	if (!changed) {
		return;
	}

	SaveProjectLibrary(ctx);
	auto after{ capture() };
	PaintEditor* paint{ this };
	EditorContext* context{ &ctx };
	auto apply = [paint, context](const auto& state) {
		paint->prefab_groups_		 = std::get<0>(state);
		paint->prefab_group_by_key_	 = std::get<1>(state);
		paint->prefab_source_		 = std::get<2>(state);
		paint->recipe_				 = std::get<3>(state);
		paint->weighted_prefab_sets_ = std::get<4>(state);
		paint->SaveProjectLibrary(*context);
	};
	ctx.undo.PushApplied(
		"Retarget Prefab References", [before = std::move(before), apply]() { apply(before); },
		[after = std::move(after), apply]() { apply(after); }
	);
}

void PaintEditor::OnPrefabDuplicated(
	EditorContext& ctx, const PrefabKey& source, const PrefabKey& duplicate
) {
	MovePrefabToGroup(ctx, duplicate, GetPrefabGroup(ctx, source));
}

void PaintEditor::OnPrefabDeleted(EditorContext& ctx, const PrefabKey& key) {
	EnsureProjectLibrary(ctx);

	auto capture = [this]() {
		return std::tuple{ prefab_groups_, prefab_group_by_key_, prefab_source_, recipe_,
						   weighted_prefab_sets_ };
	};
	auto before{ capture() };
	bool changed{ prefab_group_by_key_.erase(key.value) != 0 };

	if (prefab_source_ == key) {
		prefab_source_ = {};
		changed		   = true;
	}
	if (recipe_.checker_prefab == key) {
		recipe_.checker_prefab.reset();
		changed = true;
	}
	for (auto& threshold : recipe_.noise.thresholds) {
		if (threshold.prefab == key) {
			threshold.prefab.reset();
			changed = true;
		}
	}
	for (auto& set : weighted_prefab_sets_) {
		std::size_t size_before{ set.entries.size() };
		std::erase_if(set.entries, [&](const PaintWeightedPrefabEntry& entry) {
			return entry.prefab == key;
		});
		changed |= size_before != set.entries.size();
	}
	if (!changed) {
		return;
	}

	SaveProjectLibrary(ctx);
	auto after{ capture() };
	PaintEditor* paint{ this };
	EditorContext* context{ &ctx };
	auto apply = [paint, context](const auto& state) {
		paint->prefab_groups_		 = std::get<0>(state);
		paint->prefab_group_by_key_	 = std::get<1>(state);
		paint->prefab_source_		 = std::get<2>(state);
		paint->recipe_				 = std::get<3>(state);
		paint->weighted_prefab_sets_ = std::get<4>(state);
		paint->SaveProjectLibrary(*context);
	};
	ctx.undo.PushApplied(
		"Remove Prefab References", [before = std::move(before), apply]() { apply(before); },
		[after = std::move(after), apply]() { apply(after); }
	);
}

// An imported PNG is a project asset, not just a palette entry. Preserve its bytes
// with the import command so reimporting can be undone and redone without touching
// unrelated assets in the content browser.
[[nodiscard]] std::optional<std::vector<char>> ReadTilesetBytes(const path& file) {
	std::ifstream stream{ file, std::ios::binary | std::ios::ate };
	if (!stream) {
		return std::nullopt;
	}

	std::streampos length{ stream.tellg() };

	if (length < 0) {
		return std::nullopt;
	}

	std::vector<char> bytes(static_cast<std::size_t>(length));
	stream.seekg(0);
	if (!bytes.empty() && !stream.read(bytes.data(), static_cast<std::streamsize>(bytes.size()))) {
		return std::nullopt;
	}

	return bytes;
}

[[nodiscard]] bool WriteTilesetBytes(const path& file, const std::vector<char>& bytes) {
	std::ofstream stream{ file, std::ios::binary | std::ios::trunc };
	if (!stream) {
		return false;
	}

	if (!bytes.empty()) {
		stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	}

	stream.flush();
	return stream.good();
}

[[nodiscard]] std::optional<V2_int> ReadTilesetPngSize(const path& file) {
	std::ifstream stream{ file, std::ios::binary };
	std::array<unsigned char, 24> header{};
	if (!stream.read(reinterpret_cast<char*>(header.data()), header.size())) {
		return std::nullopt;
	}
	constexpr std::array<unsigned char, 8> signature{ 137, 80, 78, 71, 13, 10, 26, 10 };
	if (!std::equal(signature.begin(), signature.end(), header.begin())) {
		return std::nullopt;
	}
	auto read_int = [&](std::size_t offset) -> int {
		return static_cast<int>(
			(static_cast<std::uint32_t>(header[offset]) << 24) |
			(static_cast<std::uint32_t>(header[offset + 1]) << 16) |
			(static_cast<std::uint32_t>(header[offset + 2]) << 8) |
			static_cast<std::uint32_t>(header[offset + 3])
		);
	};
	V2_int size{ read_int(16), read_int(20) };
	return size.IsPositive() ? std::optional{ size } : std::nullopt;
}

int PaintEditor::ImportImageTiles(
	EditorContext& ctx, const TileImportSettings& settings, const std::string& image_path_text,
	std::string group
) {
	path image_path{ image_path_text };
	if (!fs::exists(image_path) || !IsImagePath(image_path)) {
		return 0;
	}
	if (settings.create_group_from_source) {
		group = SourceGroupName(image_path);
	}
	if (group.empty()) {
		group = "Ungrouped";
	}

	TextureKey replace_texture{ settings.replace_texture };
	if (settings.autotile) {
		if (group == "Ungrouped") {
			group = SourceGroupName(image_path);
		}
		if (group.empty() || group == "Ungrouped") {
			group = image_path.stem().string();
		}
		if (IsAutotileGroup(group)) {
			// An occupied autotile group cannot be replaced by a new import.
			// Reimporting an existing texture remains available through its group menu.
			if (std::ranges::any_of(autotile_library_, [&](const AutotileLibraryEntry& entry) {
					return entry.group == group && entry.texture != replace_texture;
				})) {
				return 0;
			}
		} else if (std::ranges::any_of(tile_library_, [&](const TileLibraryEntry& tile) {
					   return tile.group == group;
				   })) {
			// Never convert a populated ordinary group into an exclusive autotile group.
			std::string base{ SourceGroupName(image_path) };
			if (base.empty() || base == "Ungrouped") {
				base = image_path.stem().string();
			}
			group = base;
			int suffix{ 2 };
			while (std::ranges::contains(TileGroupNames(), group)) {
				group = base + " " + std::to_string(suffix++);
			}
		}
	} else if (IsAutotileGroup(group)) {
		// Individual images and ordinary tilesets cannot be added to an autotile group.
		return 0;
	}
	if (replace_texture && !settings.autotile &&
		std::ranges::contains(autotile_library_, replace_texture, &AutotileLibraryEntry::texture)) {
		return 0;
	}

	auto& assets{ ctx.editor.GetAssetManager() };
	TextureKey texture{};
	if (replace_texture) {
		auto catalog{ assets.GetCatalogAsset(replace_texture, AssetKind::Texture) };
		auto root{ ctx.editor.GetProjectRoot() };
		if (!catalog || !root || image_path.extension() != ".png" ||
			image_path.extension() != path{ catalog->source_path }.extension()) {
			return 0;
		}
		// Reject a mismatched atlas before overwriting the existing project asset.
		auto image_size{ ReadTilesetPngSize(image_path) };
		V2_int current_tile_size{ std::max(1, settings.tile_width),
								  std::max(1, settings.tile_height) };
		if (!image_size || image_size->x % current_tile_size.x != 0 ||
			image_size->y % current_tile_size.y != 0) {
			return 0;
		}
		if (settings.autotile) {
			V2_int sheet{ image_size->x / current_tile_size.x,
						  image_size->y / current_tile_size.y };
			if (!ResolveAutotileAtlasLayout(settings.autotile_format, sheet)) {
				return 0;
			}
		}
		path destination{ (*root / catalog->source_path).lexically_normal() };
		std::error_code error;
		if (!fs::equivalent(image_path, destination, error)) {
			fs::copy_file(
				image_path, destination, fs::copy_options::overwrite_existing, error
			);
			if (error) {
				return 0;
			}
		}
		texture = replace_texture;
		assets.RefreshCatalogFromDisk();
		if (!std::ranges::contains(pending_texture_reloads_, texture)) {
			pending_texture_reloads_.push_back(texture);
		}
	} else {
		auto imported{ assets.ImportAsset(image_path) };
		if (!imported) {
			return 0;
		}
		texture = TextureKey{ imported->value };
		if (auto catalog{ assets.GetCatalogAsset(*imported, AssetKind::Texture) }) {
			assets.Load(*imported, catalog->source_path);
		}
	}
	V2_int texture_size{};
	for (const auto& record : ::ptgn::impl::AssetAccessor{ assets }.GetAssets()) {
		if (record.kind == AssetKind::Texture &&
			record.key == static_cast<const AssetKey&>(texture) &&
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
	if (settings.use_filename_dimensions && !replace_texture) {
		inferred = FilenameTileDimensions(image_path);
	}
	bool slice{};
	V2_int tile_size{ texture_size };
	if (settings.autotile) {
		slice = true;
		tile_size =
			replace_texture
				? V2_int{ std::max(1, settings.tile_width), std::max(1, settings.tile_height) }
				: inferred.value_or(InferAutotileTileSize(settings.autotile_format, texture_size)
										.value_or(
											V2_int{ std::max(1, settings.tile_width),
													std::max(1, settings.tile_height) }
										));
	} else if (settings.mode == TileImportMode::Individual) {
		slice = false;
	} else if (settings.mode == TileImportMode::Tileset) {
		slice	  = true;
		tile_size = inferred.value_or(
			V2_int{ std::max(1, settings.tile_width), std::max(1, settings.tile_height) }
		);
	} else if (inferred.has_value()) {
		slice	  = true;
		tile_size = *inferred;
	} else if (settings.mode == TileImportMode::Auto) {
		V2_int requested{ std::max(1, settings.tile_width), std::max(1, settings.tile_height) };
		if (texture_size.x % requested.x == 0 && texture_size.y % requested.y == 0 &&
			(texture_size.x > requested.x || texture_size.y > requested.y)) {
			slice	  = true;
			tile_size = requested;
		}
	}

	// An invalid tile size must not erase existing slices from the library.
	V2_int proposed_size{ slice ? tile_size : texture_size };
	if (!proposed_size.IsPositive() || texture_size.x % proposed_size.x != 0 ||
		texture_size.y % proposed_size.y != 0) {
		return 0;
	}

	if (!settings.autotile && !replace_texture &&
		std::ranges::contains(autotile_library_, texture, &AutotileLibraryEntry::texture)) {
		return 0;
	}

	if (settings.autotile) {
		tile_size =
			replace_texture
				? V2_int{ std::max(1, settings.tile_width), std::max(1, settings.tile_height) }
				: inferred.value_or(InferAutotileTileSize(settings.autotile_format, texture_size)
										.value_or(
											V2_int{ std::max(1, settings.tile_width),
													std::max(1, settings.tile_height) }
										));
		if (!tile_size.IsPositive() || texture_size.x % tile_size.x != 0 ||
			texture_size.y % tile_size.y != 0) {
			return 0;
		}
		V2_int sheet{ texture_size.x / tile_size.x, texture_size.y / tile_size.y };
		if (!ResolveAutotileAtlasLayout(settings.autotile_format, sheet)) {
			return 0;
		}
		tile_slice_settings_.insert_or_assign(
			texture.value, TileSliceSettings{ .tile_size = tile_size }
		);
		std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
			return entry.texture == texture;
		});
		bool show_slices{};
		std::string tileset_name{ image_path.stem().string() };
		for (const auto& entry : autotile_library_) {
			if (entry.texture == texture) {
				show_slices = entry.show_slices;
				if (replace_texture) {
					tileset_name = entry.name;
				}
				break;
			}
		}
		std::erase_if(autotile_library_, [&](const AutotileLibraryEntry& entry) {
			return entry.texture == texture;
		});
		autotile_library_.push_back(
			AutotileLibraryEntry{
				.name		 = tileset_name,
				.texture	 = texture,
				.format		 = settings.autotile_format,
				.group		 = group,
				.show_slices = show_slices,
			}
		);
		std::erase(tile_groups_, group);
		if (!IsAutotileGroup(group)) {
			autotile_groups_.push_back(group);
		}
		if (!replace_texture) {
			std::erase_if(autotile_rule_sets_, [&](const PaintAutotileRuleSet& rules) {
				return rules.texture == texture;
			});
			recipe_.autotile_texture = texture;
			recipe_.autotile_format	 = settings.autotile_format;
			recipe_.source_kind		 = PaintSourceKind::Autotile;
			selected_tile_entry_id_.clear();
			inspected_texture_ = texture;
			inspected_slice_   = {};
			if (!ResolveAutotileRuleSet(ctx)) {
				return 0;
			}
		} else {
			// Keep the ruleset ID referenced by previously painted terrain.
			for (auto& rules : autotile_rule_sets_) {
				if (rules.texture == texture) {
					rules.format = settings.autotile_format;
					RefreshAutotileRuleSet(ctx, rules);
				}
			}
		}
		SaveProjectLibrary(ctx);
		return 1;
	}

	std::erase_if(autotile_library_, [&](const AutotileLibraryEntry& entry) {
		return entry.texture == texture;
	});
	std::erase_if(autotile_rule_sets_, [&](const PaintAutotileRuleSet& rules) {
		return rules.texture == texture;
	});
	std::unordered_map<std::string, TileLibraryEntry> previous_tiles;
	if (replace_texture) {
		for (const auto& entry : tile_library_) {
			if (entry.texture == texture) {
				previous_tiles.emplace(entry.id, entry);
			}
		}
	}
	TileSliceSettings slicing{ .tile_size = slice ? tile_size : texture_size };
	tile_slice_settings_.insert_or_assign(texture.value, slicing);
	std::erase_if(tile_library_, [&](const TileLibraryEntry& entry) {
		return entry.texture == texture;
	});

	int columns{ slice ? std::max(0, texture_size.x / std::max(1, slicing.tile_size.x)) : 1 };
	int rows{ slice ? std::max(0, texture_size.y / std::max(1, slicing.tile_size.y)) : 1 };
	int count{};
	for (int y{}; y < rows; ++y) {
		for (int x{}; x < columns; ++x) {
			V2_int coord{ x, y };
			std::string id{ TileEntryId(texture, coord) };
			std::string name{ slice ? image_path.stem().string() + "_" + std::to_string(count)
									: image_path.stem().string() };
			std::string entry_group{ group };
			if (auto old{ previous_tiles.find(id) }; old != previous_tiles.end()) {
				name		= old->second.name;
				entry_group = old->second.group;
			}
			tile_library_.push_back(
				TileLibraryEntry{ .id	   = id,
								  .name	   = name,
								  .texture = texture,
								  .slice   = coord,
								  .group   = entry_group }
			);
			++count;
		}
	}
	if (count > 0) {
		if (group != "Ungrouped" && !std::ranges::contains(tile_groups_, group)) {
			tile_groups_.push_back(group);
		}
		if (!replace_texture) {
			selected_tile_entry_id_ = tile_library_.back().id;
			recipe_.source_kind		= PaintSourceKind::Single;
			inspected_texture_		= texture;
			inspected_slice_		= tile_library_.back().slice;
		}
		SaveProjectLibrary(ctx);
	}
	return count;
}

int PaintEditor::ImportTiles(EditorContext& ctx, const TileImportSettings& settings) {
	if (settings.path.empty()) {
		return 0;
	}
	path source{ settings.path };
	if (!fs::exists(source)) {
		return 0;
	}
	std::string ext{ source.extension().string() };
	std::ranges::transform(ext, ext.begin(), [](unsigned char c) {
		return static_cast<char>(std::tolower(c));
	});
	std::string group{ settings.target_group.empty() ? "Ungrouped" : settings.target_group };

	// Importing multiple sheets into a single exclusive group would silently replace
	// the previous sheet with the last selected file. Require one replacement at a time.
	if (settings.autotile && settings.paths.size() > 1 && !settings.create_group_from_source &&
		group != "Ungrouped") {
		return 0;
	}

	path replaced_asset_path;
	std::optional<std::vector<char>> previous_asset_bytes;
	if (settings.replace_texture) {
		auto& assets{ ctx.editor.GetAssetManager() };
		auto catalog{ assets.GetCatalogAsset(settings.replace_texture, AssetKind::Texture) };
		auto root{ ctx.editor.GetProjectRoot() };
		if (!catalog || !root) {
			return 0;
		}

		replaced_asset_path	 = (*root / catalog->source_path).lexically_normal();
		previous_asset_bytes = ReadTilesetBytes(replaced_asset_path);
		if (!previous_asset_bytes) {
			return 0;
		}
	}

	// Tile imports can replace existing slices as well as create new entries.
	// Record the complete library change as one command so undo restores both
	// the original entries and their group memberships.
	struct ImportState {
		std::vector<TileLibraryEntry> tiles;
		std::vector<AutotileLibraryEntry> autotiles;
		std::vector<PaintAutotileRuleSet> rulesets;
		std::vector<std::string> groups;
		std::vector<std::string> autotile_groups;
		decltype(tile_slice_settings_) slices;
		std::string selected_tile;
		std::optional<TextureKey> inspected_texture;
		V2_int inspected_slice{};
		PaintRecipeState recipe;
		PaintTileSource tile_source;
	};

	auto capture = [this]() {
		return ImportState{
			.tiles			   = tile_library_,
			.autotiles		   = autotile_library_,
			.rulesets		   = autotile_rule_sets_,
			.groups			   = tile_groups_,
			.autotile_groups   = autotile_groups_,
			.slices			   = tile_slice_settings_,
			.selected_tile	   = selected_tile_entry_id_,
			.inspected_texture = inspected_texture_,
			.inspected_slice   = inspected_slice_,
			.recipe			   = recipe_,
			.tile_source	   = tile_source_,
		};
	};

	ImportState before{ capture() };
	auto rollback_import = [&]() {
		if (previous_asset_bytes) {
			auto current_bytes{ ReadTilesetBytes(replaced_asset_path) };

			if (!current_bytes || *current_bytes != *previous_asset_bytes) {
				if (WriteTilesetBytes(replaced_asset_path, *previous_asset_bytes)) {
					ctx.editor.GetAssetManager().RefreshCatalogFromDisk();
					if (!std::ranges::contains(
							pending_texture_reloads_, settings.replace_texture
						)) {
						pending_texture_reloads_.push_back(settings.replace_texture);
					}
				}
			}
		}
		tile_library_			= before.tiles;
		autotile_library_		= before.autotiles;
		autotile_rule_sets_		= before.rulesets;
		tile_groups_			= before.groups;
		autotile_groups_		= before.autotile_groups;
		tile_slice_settings_	= before.slices;
		selected_tile_entry_id_ = before.selected_tile;
		inspected_texture_		= before.inspected_texture;
		inspected_slice_		= before.inspected_slice;
		recipe_					= before.recipe;
		tile_source_			= before.tile_source;
		SaveProjectLibrary(ctx);
	};
	auto finish_import = [&](int count) {
		if (count <= 0) {
			rollback_import();
			return count;
		}

		ImportState after{ capture() };
		std::shared_ptr<const std::vector<char>> original_png;
		std::shared_ptr<const std::vector<char>> imported_png;

		if (previous_asset_bytes) {
			auto current_bytes{ ReadTilesetBytes(replaced_asset_path) };

			if (!current_bytes) {
				rollback_import();
				return 0;
			}

			original_png =
				std::make_shared<const std::vector<char>>(std::move(*previous_asset_bytes));
			imported_png = std::make_shared<const std::vector<char>>(std::move(*current_bytes));
		}

		PaintEditor* paint{ this };
		EditorContext* context{ &ctx };
		auto apply = [paint, context, replaced_asset_path, key = settings.replace_texture](
						 const ImportState& state,
						 const std::shared_ptr<const std::vector<char>>& png
					 ) {
			if (png) {
				if (!WriteTilesetBytes(replaced_asset_path, *png)) {
					return;
				}

				context->editor.GetAssetManager().RefreshCatalogFromDisk();
				if (!std::ranges::contains(paint->pending_texture_reloads_, key)) {
					paint->pending_texture_reloads_.push_back(key);
				}
			}
			paint->tile_library_		   = state.tiles;
			paint->autotile_library_	   = state.autotiles;
			paint->autotile_rule_sets_	   = state.rulesets;
			paint->tile_groups_			   = state.groups;
			paint->autotile_groups_		   = state.autotile_groups;
			paint->tile_slice_settings_	   = state.slices;
			paint->selected_tile_entry_id_ = state.selected_tile;
			paint->inspected_texture_	   = state.inspected_texture;
			paint->inspected_slice_		   = state.inspected_slice;
			paint->recipe_				   = state.recipe;
			paint->tile_source_			   = state.tile_source;
			paint->SaveProjectLibrary(*context);
		};
		ctx.undo.PushApplied(
			settings.replace_texture ? "Reimport Tileset" : "Import Tiles",
			[before = std::move(before), original_png, apply]() { apply(before, original_png); },
			[after = std::move(after), imported_png, apply]() { apply(after, imported_png); }
		);
		return count;
	};
	if (ext == ".tsx" || ext == ".tsj" || ext == ".json") {
		TiledTilesetInfo info{ ParseTiledTileset(source) };
		int total{};
		for (const auto& image_name : info.images) {
			path image{ image_name };
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
			}
			total += ImportImageTiles(
				ctx, child, image.lexically_normal().string(),
				settings.create_group_from_source ? SourceGroupName(source) : group
			);
		}
		return finish_import(total);
	}
	int total{};
	if (!settings.paths.empty()) {
		for (const std::string& selected : settings.paths) {
			path item{ selected };
			if (IsImagePath(item)) {
				total += ImportImageTiles(ctx, settings, item.lexically_normal().string(), group);
			}
		}
	} else {
		total = ImportImageTiles(ctx, settings, source.lexically_normal().string(), group);
	}
	return finish_import(total);
}

void PaintEditor::DrawImportPopup(EditorContext& ctx) {
	if (import_popup_requested_) {
		// Preserve the manually selected tile proportions when the dialog opens.
		import_tile_locked_ratio_width_	 = std::max(1, import_settings_.tile_width);
		import_tile_locked_ratio_height_ = std::max(1, import_settings_.tile_height);
		ImGui::OpenPopup("Import Tiles");
		import_popup_requested_ = false;
	}

	if (!ImGui::IsPopupOpen("Import Tiles")) {
		return;
	}

	// Match the centered, immovable Rename modal using the shared dialog scale.
	if (const ImGuiViewport* viewport{ ImGui::GetMainViewport() }) {
		ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Always, ImVec2{ 0.5f, 0.5f });
	}

	constexpr float modal_scale{ kEditorModalScale };
	const ImGuiStyle& style{ ImGui::GetStyle() };
	ImGui::PushStyleVar(
		ImGuiStyleVar_WindowPadding,
		ImVec2{ style.WindowPadding.x * modal_scale, style.WindowPadding.y * modal_scale }
	);
	ImGui::PushStyleVar(
		ImGuiStyleVar_FramePadding,
		ImVec2{ style.FramePadding.x * modal_scale, style.FramePadding.y * modal_scale }
	);
	ImGui::PushStyleVar(
		ImGuiStyleVar_ItemSpacing,
		ImVec2{ style.ItemSpacing.x * modal_scale, style.ItemSpacing.y * modal_scale }
	);
	ImGui::PushStyleVar(
		ImGuiStyleVar_ItemInnerSpacing,
		ImVec2{ style.ItemInnerSpacing.x * modal_scale, style.ItemInnerSpacing.y * modal_scale }
	);

	if (!ImGui::BeginPopupModal(
			"Import Tiles", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove
		)) {
		ImGui::PopStyleVar(4);
		return;
	}

	ImGui::SetWindowFontScale(modal_scale);

	// Every field begins at the same horizontal position, regardless of its label length.
	float spacing{ ImGui::GetStyle().ItemSpacing.x };
	float field_spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
	float label_width{ std::max(
		190.0f * modal_scale,
		ImGui::CalcTextSize("Create group from source name").x + 8.0f * modal_scale
	) };
	float control_width{ 380.0f * modal_scale };
	float value_x{ ImGui::GetCursorPosX() + label_width + spacing };
	auto row = [&](const char* label) {
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(label);
		ImGui::SameLine();
		ImGui::SetCursorPosX(value_x);
	};

	row("Source");
	float browse_width{ std::max(
		88.0f * modal_scale, ImGui::CalcTextSize("Browse...").x +
								 2.0f * ImGui::GetStyle().FramePadding.x + 8.0f * modal_scale
	) };
	if (ImGui::Button("Browse...", ImVec2{ browse_width, 0.0f })) {
		auto result{ ctx.editor.GetWindow().file.OpenFiles(TileImportOptions()) };

		if (!result.has_value()) {
			import_status_ = "File picker failed: " + result.error();
		} else if (result->has_value() && !result->value().empty()) {
			import_settings_.paths.clear();

			for (const auto& file : result->value()) {
				import_settings_.paths.push_back(file.string());
			}

			import_settings_.path = import_settings_.paths.front();
			import_status_.clear();
		}
	}

	std::string display_source{ "No file selected" };

	if (!import_settings_.path.empty()) {
		display_source = path{ import_settings_.path }.filename().string();
		if (import_settings_.paths.size() > 1) {
			display_source += " (+" + std::to_string(import_settings_.paths.size() - 1) + ")";
		}
	}

	ImGui::SameLine(0.0f, field_spacing);
	ImGui::SetNextItemWidth(std::max(1.0f, control_width - browse_width - field_spacing));
	ImGui::InputText("##TileImportSource", &display_source, ImGuiInputTextFlags_ReadOnly);

	if (ImGui::IsItemHovered() && !import_settings_.path.empty()) {
		ImGui::BeginTooltip();
		if (import_settings_.paths.empty()) {
			ImGui::TextUnformatted(import_settings_.path.c_str());
		} else {
			for (const std::string& source : import_settings_.paths) {
				ImGui::TextUnformatted(source.c_str());
			}
		}
		ImGui::EndTooltip();
	}

	auto valid_import_file = [](const std::string& source) -> bool {
		if (source.empty()) {
			return false;
		}

		path file{ source };
		std::error_code error;

		if (!fs::is_regular_file(file, error) || error) {
			return false;
		}

		if (IsImagePath(file)) {
			return true;
		}

		std::string extension{ file.extension().string() };
		std::ranges::transform(extension, extension.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		if (extension == ".tsx" || extension == ".tsj" || extension == ".json") {
			return !ParseTiledTileset(file).images.empty();
		}

		return false;
	};

	bool valid_source{ valid_import_file(import_settings_.path) };

	for (const auto& source : import_settings_.paths) {
		valid_source = valid_source && valid_import_file(source);
	}

	bool exclusive_target{ IsAutotileGroup(import_settings_.target_group) };

	if (exclusive_target) {
		import_settings_.autotile				  = true;
		import_settings_.create_group_from_source = false;
	}

	row("Autotile");
	ImGui::BeginDisabled(exclusive_target);
	ImGui::Checkbox("##ImportAutotile", &import_settings_.autotile);
	ImGui::EndDisabled();

	if (import_settings_.autotile) {
		row("Layout");
		int format{ static_cast<int>(import_settings_.autotile_format) };
		ImGui::SetNextItemWidth(control_width);

		if (ImGui::Combo(
				"##ImportTileLayout", &format,
				"15 pieces (5x3 / 4x4)\0"
				"47 pieces (8x6)\0"
				"16 subset (4x4)\0"
				"Dual grid (15 or 16)\0"
			)) {
			import_settings_.autotile_format = static_cast<PaintAutotileFormat>(format);
		}
	} else {
		row("Import Mode");
		constexpr std::array<const char*, 3> mode_names{ "Auto", "Tileset", "Individual" };
		constexpr std::array<const char*, 3> mode_tooltips{
			"Detect whether the image contains a tileset.",
			"Split the image into tiles of the chosen size.",
			"Import the entire image as a single tile."
		};
		int current_mode{ static_cast<int>(import_settings_.mode) };
		ImGui::SetNextItemWidth(control_width);

		if (ImGui::BeginCombo("##ImportTileMode", mode_names[static_cast<std::size_t>(std::clamp(current_mode, 0, 2))])) {
			for (std::size_t i{}; i < mode_names.size(); ++i) {
				bool selected{ current_mode == static_cast<int>(i) };

				if (ImGui::Selectable(mode_names[i], selected)) {
					import_settings_.mode = static_cast<TileImportMode>(i);
				}

				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s", mode_tooltips[i]);
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
	}

	// A filename suffix such as _16x16 always supplies the tile dimensions.
	// When importing several images, show fixed dimensions only if all agree.
	std::optional<V2_int> filename_dimensions;
	bool same_filename_dimensions{ valid_source };

	if (valid_source) {
		if (import_settings_.paths.empty()) {
			filename_dimensions = FilenameTileDimensions(path{ import_settings_.path });
		} else {
			filename_dimensions = FilenameTileDimensions(path{ import_settings_.paths.front() });

			for (const auto& source : import_settings_.paths) {
				if (FilenameTileDimensions(path{ source }) != filename_dimensions) {
					same_filename_dimensions = false;
					break;
				}
			}
		}
	}

	if (!same_filename_dimensions) {
		filename_dimensions.reset();
	}

	import_settings_.use_filename_dimensions = true;
	bool single_image_mode{ !import_settings_.autotile &&
							import_settings_.mode == TileImportMode::Individual };
	bool fixed_tile_size{ filename_dimensions.has_value() || single_image_mode };
	int width{ filename_dimensions ? filename_dimensions->x : import_settings_.tile_width };
	int height{ filename_dimensions ? filename_dimensions->y : import_settings_.tile_height };

	row("Tile Size");
	float lock_width{ ImGui::GetFrameHeight() };
	float field_width{ std::max(1.0f, (control_width - lock_width - 2.0f * field_spacing) * 0.5f) };
	ImGui::BeginDisabled(fixed_tile_size);
	ImGui::SetNextItemWidth(field_width);
	bool width_changed{ ImGui::DragInt("##ImportTileWidth", &width, 1.0f, 1, 8192, "W: %d") };
	ImGui::SameLine(0.0f, field_spacing);
	ImGui::SetNextItemWidth(field_width);
	bool height_changed{ ImGui::DragInt("##ImportTileHeight", &height, 1.0f, 1, 8192, "H: %d") };
	ImGui::EndDisabled();

	ImGui::SameLine(0.0f, field_spacing);
	// The icon button is exactly one frame high and must end at the same
	// right edge as every other value field in this dialog.
	ImGui::SetCursorPosX(value_x + control_width - lock_width);
	ImGui::BeginDisabled(fixed_tile_size);
	bool lock_changed{ DrawEditorIconButton(
		"##ImportTileAspectLock",
		import_tile_aspect_locked_ ? EditorIcon::Locked : EditorIcon::Unlocked,
		EditorIconButtonOptions{
			.tooltip	 = "Preserve the current tile aspect ratio.",
			.muted		 = !import_tile_aspect_locked_,
			.icon_extent = ImGui::GetTextLineHeight() + 2.0f,
		}
	) };

	if (lock_changed) {
		import_tile_aspect_locked_ = !import_tile_aspect_locked_;

		if (import_tile_aspect_locked_) {
			// Lock the ratio at the moment the user enables the lock.
			// Enabling the lock must not change the current tile dimensions.
			import_tile_locked_ratio_width_	 = std::max(1, width);
			import_tile_locked_ratio_height_ = std::max(1, height);
		}
	}
	ImGui::EndDisabled();

	if (!fixed_tile_size && (width_changed || height_changed)) {
		if (import_tile_aspect_locked_) {
			// Use the captured ratio rather than the previously rounded dimensions,
			// preventing successive edits from accumulating rounding errors.
			if (width_changed) {
				height = std::clamp(
					static_cast<int>(std::lround(
						static_cast<double>(width) * import_tile_locked_ratio_height_ /
						import_tile_locked_ratio_width_
					)),
					1, 8192
				);
			} else if (height_changed) {
				width = std::clamp(
					static_cast<int>(std::lround(
						static_cast<double>(height) * import_tile_locked_ratio_width_ /
						import_tile_locked_ratio_height_
					)),
					1, 8192
				);
			}
		}

		import_settings_.tile_width	 = std::clamp(width, 1, 8192);
		import_settings_.tile_height = std::clamp(height, 1, 8192);
	}

	row("Create group from source name");
	ImGui::BeginDisabled(exclusive_target);
	ImGui::Checkbox("##ImportGroupFromSource", &import_settings_.create_group_from_source);
	ImGui::EndDisabled();

	if (!import_settings_.create_group_from_source) {
		row("Group");
		auto groups{ TileGroupNames() };
		ImGui::SetNextItemWidth(control_width);

		if (ImGui::BeginCombo("##ImportTileGroup", import_settings_.target_group.c_str())) {
			for (const auto& group : groups) {
				if (group != "Ungrouped" && IsAutotileGroup(group) != import_settings_.autotile) {
					continue;
				}

				if (ImGui::Selectable(group.c_str(), group == import_settings_.target_group)) {
					import_settings_.target_group = group;
				}
			}
			ImGui::EndCombo();
		}
	}

	if (exclusive_target) {
		ImGui::TextDisabled("This group contains one autotile tileset.");
	}

	if (!import_status_.empty()) {
		ImGui::TextDisabled("%s", import_status_.c_str());
	}

	// Match the horizontal popup padding above the buttons. The default
	// window padding supplies the same gap below them; no extra bottom dummy.
	float desired_gap{ ImGui::GetStyle().WindowPadding.x };
	float additional_gap{ std::max(0.0f, desired_gap - ImGui::GetStyle().ItemSpacing.y) };
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + additional_gap);
	float button_width{ std::max(1.0f, (ImGui::GetContentRegionAvail().x - spacing) * 0.5f) };

	if (ImGui::Button("Cancel", ImVec2{ button_width, 0.0f })) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::SameLine(0.0f, spacing);
	ImGui::BeginDisabled(!valid_source);
	bool import_pressed{ ImGui::Button("Import", ImVec2{ button_width, 0.0f }) };
	ImGui::EndDisabled();

	if (import_pressed) {
		int count{ ImportTiles(ctx, import_settings_) };
		import_status_ =
			count > 0 ? "Imported " + std::to_string(count) + " tile" + (count == 1 ? "." : "s.")
					  : "Nothing was imported.";

		if (count > 0) {
			import_settings_.paths.clear();
			ImGui::CloseCurrentPopup();
		}
	}

	ImGui::SetWindowFontScale(1.0f);
	ImGui::EndPopup();
	ImGui::PopStyleVar(4);
}

bool PaintEditor::DrawTilesPanel(EditorContext& ctx) {
	// Rebuild textures at the beginning of the next frame, after the previous
	// frame has finished consuming its cached preview texture identifiers.
	for (const TextureKey& key : pending_texture_reloads_) {
		ctx.editor.GetAssetManager().ReloadTextureFromDisk(key);
	}
	pending_texture_reloads_.clear();
	EnsureLocalState(ctx);
	bool visible{ ImGui::Begin("Tiles###PrefabsWindow") };
	bool focused{ visible && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) };
	if (focused) {
		ctx.editor.GetSceneHierarchyPanel().SetActiveTab(SceneHierarchyTab::Tiles);
	}
	if (!visible) {
		ImGui::End();
		return false;
	}
	EnsureProjectLibrary(ctx);

	if (focused && !ImGui::GetIO().WantTextInput && !selected_tile_entry_id_.empty() &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		DeleteTileLibraryEntry(ctx, selected_tile_entry_id_);
	}

	auto capture_groups = [&]() {
		GroupSnapshot result{ CaptureTileGroups(tile_groups_, tile_library_) };
		for (const auto& entry : autotile_library_) {
			result.members.insert_or_assign("auto:" + entry.texture.value, entry.group);
		}
		result.autotile_groups = autotile_groups_;
		return result;
	};
	auto apply_groups = [this](EditorContext& context, const GroupSnapshot& state) {
		tile_groups_	 = state.groups;
		autotile_groups_ = state.autotile_groups;
		for (auto& entry : tile_library_) {
			if (auto it{ state.members.find(entry.id) }; it != state.members.end()) {
				entry.group = it->second;
			}
		}
		for (auto& entry : autotile_library_) {
			if (auto it{ state.members.find("auto:" + entry.texture.value) };
				it != state.members.end()) {
				entry.group = it->second;
			}
		}
		SaveProjectLibrary(context);
	};
	auto commit_groups = [&](std::string label, GroupSnapshot before) {
		SaveProjectLibrary(ctx);
		RecordGroupChange(ctx, std::move(label), std::move(before), capture_groups(), apply_groups);
	};

	auto create_group = [&]() {
		GroupSnapshot before{ capture_groups() };
		auto groups{ TileGroupNames() };
		tile_groups_.push_back(NextDefaultGroupName(groups));
		std::ranges::sort(tile_groups_);
		commit_groups("Create Tile Group", std::move(before));
	};

	auto add_tiles = [&](std::string_view group) {
		import_settings_.target_group			  = group;
		import_settings_.create_group_from_source = false;
		import_settings_.replace_texture		  = {};
		import_settings_.autotile				  = IsAutotileGroup(group);
		if (import_settings_.autotile) {
			for (const auto& entry : autotile_library_) {
				if (entry.group == group) {
					import_settings_.autotile_format = entry.format;
					if (const TileSliceSettings* slicing{ FindSliceSettings(entry.texture) }) {
						import_settings_.tile_width	 = slicing->tile_size.x;
						import_settings_.tile_height = slicing->tile_size.y;
					}
					break;
				}
			}
		}
		import_popup_requested_ = true;
	};

	ImGui::SetNextItemWidth(-FLT_MIN);
	ImGui::InputTextWithHint("##TileSearch", "Filter: grass, terrain, -stone", &tile_search_);
	if (ImGui::IsItemHovered()) {
		ImGui::BeginTooltip();
		ImGui::TextUnformatted("Tile filter syntax:");
		ImGui::Separator();
		ImGui::TextUnformatted("grass");
		ImGui::SameLine();
		ImGui::TextDisabled("Tile name or texture contains \"grass\"");
		ImGui::TextUnformatted("terrain");
		ImGui::SameLine();
		ImGui::TextDisabled("Group contains \"terrain\"");
		ImGui::TextUnformatted("-stone");
		ImGui::SameLine();
		ImGui::TextDisabled("Tile name, texture and group do not contain \"stone\"");
		ImGui::Spacing();
		ImGui::TextDisabled("Separate filters with commas.");
		ImGui::TextDisabled("Positive filters use OR; all exclusions apply.");
		ImGui::TextDisabled("Matching ignores letter case.");
		ImGui::EndTooltip();
	}
	ImGui::Separator();

	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::optional<std::pair<std::string, std::string>> pending_group_move;
	std::optional<TextureKey> pending_reimport;
	static std::string group_to_rename;
	static RenameModalState group_rename_modal;
	auto matches_tile = [&](const TileLibraryEntry& entry, std::string_view group) {
		return MatchesTextFilter(tile_search_, { group, entry.name, entry.texture.value });
	};

	for (const std::string& group : TileGroupNames()) {
		bool group_matches{ MatchesTextFilter(tile_search_, { group }) };
		bool has_matching{ std::ranges::any_of(
							   tile_library_,
							   [&](const TileLibraryEntry& entry) {
								   return entry.group == group && matches_tile(entry, group);
							   }
						   ) ||
						   std::ranges::any_of(
							   autotile_library_, [&](const AutotileLibraryEntry& entry) {
								   return entry.group == group &&
										  MatchesTextFilter(
											  tile_search_,
											  { group, entry.name, entry.texture.value }
										  );
							   }
						   ) };
		if (!has_matching && !group_matches) {
			continue;
		}
		ImGui::PushID(group.c_str());
		bool open{ ImGui::TreeNodeEx(
			"##TileGroup", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s",
			group.c_str()
		) };
		if (!IsAutotileGroup(group) && ImGui::BeginDragDropTarget()) {
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
		if (ImGui::BeginPopupContextItem("TileGroupContext")) {
			if (IsAutotileGroup(group)) {
				auto atlas{ std::ranges::find_if(
					autotile_library_,
					[&](const AutotileLibraryEntry& entry) { return entry.group == group; }
				) };

				if (atlas != autotile_library_.end()) {
					if (ImGui::MenuItem(
							atlas->show_slices ? "View as Autotile" : "View as Individual Tiles"
						)) {
						TextureKey texture{ atlas->texture };
						bool before{ atlas->show_slices };

						atlas->show_slices = !before;
						SaveProjectLibrary(ctx);

						PaintEditor* paint{ this };
						EditorContext* context{ &ctx };
						auto apply = [paint, context, texture](bool show_slices) {
							auto entry{ std::ranges::find(
								paint->autotile_library_, texture, &AutotileLibraryEntry::texture
							) };

							if (entry != paint->autotile_library_.end()) {
								entry->show_slices = show_slices;
								paint->SaveProjectLibrary(*context);
							}
						};
						ctx.undo.PushApplied(
							before ? "View as Autotile" : "View as Individual Tiles",
							[apply, before]() { apply(before); },
							[apply, before]() { apply(!before); }
						);
					}

					if (ImGui::MenuItem("Reimport Tileset")) {
						pending_reimport = atlas->texture;
					}
				} else if (ImGui::MenuItem("Import Autotile Tileset")) {
					add_tiles(group);
				}
			} else if (ImGui::MenuItem("Add Tiles")) {
				add_tiles(group);
			}
			if (group != "Ungrouped") {
				ImGui::Separator();
				if (ImGui::MenuItem("Rename")) {
					group_to_rename = group;
					group_rename_modal.Begin(group);
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Delete")) {
					if (IsAutotileGroup(group)) {
						// Deleting an exclusive group removes its palette atlas rather than
						// moving that atlas into an ordinary or shared group.
						const auto old_tiles{ autotile_library_ };
						const auto old_groups{ autotile_groups_ };
						const auto old_recipe{ recipe_ };
						std::erase_if(autotile_library_, [&](const AutotileLibraryEntry& entry) {
							return entry.group == group;
						});
						std::erase(autotile_groups_, group);
						if (recipe_.source_kind == PaintSourceKind::Autotile &&
							!std::ranges::contains(
								autotile_library_, recipe_.autotile_texture,
								&AutotileLibraryEntry::texture
							)) {
							recipe_.autotile_texture = {};
							recipe_.source_kind		 = PaintSourceKind::Single;
						}
						SaveProjectLibrary(ctx);
						const auto new_tiles{ autotile_library_ };
						const auto new_groups{ autotile_groups_ };
						const auto new_recipe{ recipe_ };
						PaintEditor* paint{ this };
						EditorContext* context{ &ctx };
						auto restore = [paint, context](
										   const auto& tiles, const auto& groups, const auto& recipe
									   ) {
							paint->autotile_library_ = tiles;
							paint->autotile_groups_	 = groups;
							paint->recipe_			 = recipe;
							paint->SaveProjectLibrary(*context);
						};
						ctx.undo.PushApplied(
							"Delete Autotile Group",
							[restore, old_tiles, old_groups, old_recipe]() {
								restore(old_tiles, old_groups, old_recipe);
							},
							[restore, new_tiles, new_groups, new_recipe]() {
								restore(new_tiles, new_groups, new_recipe);
							}
						);
					} else {
						GroupSnapshot before{ capture_groups() };
						for (auto& entry : tile_library_) {
							if (entry.group == group) {
								entry.group = "Ungrouped";
							}
						}
						std::erase(tile_groups_, group);
						commit_groups("Delete Tile Group", std::move(before));
					}
				}
			}
			ImGui::EndPopup();
		}

		if (open) {
			int shown{};
			float cell{ 48.0f };
			int per_row{
				std::max(1, static_cast<int>(ImGui::GetContentRegionAvail().x / (cell + 5.0f)))
			};
			std::optional<std::string> delete_id;
			for (auto& entry : autotile_library_) {
				if (entry.group != group ||
					!MatchesTextFilter(tile_search_, { group, entry.name, entry.texture.value })) {
					continue;
				}
				if (entry.show_slices) {
					shown = 0;
					ImGui::PushID(entry.texture.value.c_str());
					const TileSliceSettings* settings{ FindSliceSettings(entry.texture) };
					V2_int dimensions{};
					for (const auto& record : records) {
						if (record.kind == AssetKind::Texture &&
							record.key == static_cast<const AssetKey&>(entry.texture) &&
							record.metadata.dimensions) {
							dimensions = *record.metadata.dimensions;
							break;
						}
					}
					if (settings && settings->tile_size.IsPositive() && dimensions.IsPositive()) {
						V2_int sheet{ dimensions.x / settings->tile_size.x,
									  dimensions.y / settings->tile_size.y };
						if (auto layout{ ResolveAutotileAtlasLayout(entry.format, sheet) }) {
							int row_index{};
							for (int slot{}; slot < layout->source_count; ++slot) {
								if (row_index % per_row != 0) {
									ImGui::SameLine();
								}
								ImGui::PushID(slot);
								PaintTileSource source{ MakeTileSource(
									ctx, entry.texture, AutotileSourceSlice(*layout, slot)
								) };
								ImVec2 start{ ImGui::GetCursorScreenPos() };
								ImGui::InvisibleButton("##AtlasTile", { cell, cell });
								ImVec2 end{ start.x + cell, start.y + cell };
								auto* draw{ ImGui::GetWindowDrawList() };
								draw->AddRectFilled(
									start, end, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f
								);
								for (const auto& record : records) {
									if (record.kind != AssetKind::Texture ||
										record.key != static_cast<const AssetKey&>(entry.texture) ||
										!record.preview || !source) {
										continue;
									}
									const auto& uv{ source.texture_coordinates };
									draw->AddImageQuad(
										static_cast<ImTextureID>(record.preview->texture), start,
										{ end.x, start.y }, end, { start.x, end.y }, ToImGui(uv[0]),
										ToImGui(uv[1]), ToImGui(uv[2]), ToImGui(uv[3])
									);
									break;
								}
								if (recipe_.source_kind == PaintSourceKind::Single &&
									tile_source_ == source) {
									draw->AddRect(start, end, kSelection, 3.0f, 0, 2.0f);
								}
								if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
									tile_source_		= source;
									recipe_.source_kind = PaintSourceKind::Single;
									selected_tile_entry_id_.clear();
									inspected_texture_ = entry.texture;
									inspected_slice_   = source.slice;
									tool_			   = PaintTool::Brush;
								}
								if (ImGui::IsItemHovered()) {
									ImGui::SetTooltip(
										"%s | tile %d | Manual painting", entry.name.c_str(), slot
									);
								}
								ImGui::PopID();
								++row_index;
							}
						}
					}
					ImGui::PopID();
					continue;
				}
				if (shown % per_row != 0) {
					ImGui::SameLine();
				}
				ImGui::PushID(entry.texture.value.c_str());
				ImVec2 p0{ ImGui::GetCursorScreenPos() };
				ImGui::InvisibleButton("##Autotile", { cell, cell });
				ImVec2 p1{ p0.x + cell, p0.y + cell };
				auto* draw{ ImGui::GetWindowDrawList() };
				draw->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
				auto record_it{ std::ranges::find_if(records, [&](const auto& r) {
					return r.kind == AssetKind::Texture &&
						   r.key == static_cast<const AssetKey&>(entry.texture);
				}) };
				if (record_it != records.end() && record_it->preview.has_value()) {
					const auto& preview{ *record_it->preview };
					V2_int size{ preview.size };
					if (size.IsPositive()) {
						float scale{ std::min(
							cell / static_cast<float>(size.x), cell / static_cast<float>(size.y)
						) };
						ImVec2 extent{ static_cast<float>(size.x) * scale,
									   static_cast<float>(size.y) * scale };
						ImVec2 a{ p0.x + (cell - extent.x) * 0.5f,
								  p0.y + (cell - extent.y) * 0.5f };
						draw->AddImage(
							static_cast<ImTextureID>(preview.texture), a,
							{ a.x + extent.x, a.y + extent.y }
						);
					}
				}
				bool selected{ recipe_.source_kind == PaintSourceKind::Autotile &&
							   recipe_.autotile_texture == entry.texture &&
							   recipe_.autotile_format == entry.format };
				if (selected) {
					draw->AddRect(p0, p1, kSelection, 3.0f, 0, 2.0f);
				}
				// Keep the distinction visible even in a small thumbnail.
				draw->AddText({ p0.x + 3.0f, p0.y + 2.0f }, IM_COL32(255, 255, 255, 255), "A");
				if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
					recipe_.source_kind		 = PaintSourceKind::Autotile;
					recipe_.autotile_texture = entry.texture;
					recipe_.autotile_format	 = entry.format;
					tile_source_			 = {};
					selected_tile_entry_id_.clear();
					inspected_texture_ = entry.texture;
					inspected_slice_   = {};
					tool_			   = PaintTool::Brush;
					ResolveAutotileRuleSet(ctx);
				}
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip(
						"%s\n%s\nClick to paint automatically.", entry.name.c_str(),
						AutotileFormatName(entry.format)
					);
				}
				ImGui::PopID();
				++shown;
			}
			for (auto& entry : tile_library_) {
				if (entry.group != group || !matches_tile(entry, group)) {
					continue;
				}
				if (shown % per_row != 0) {
					ImGui::SameLine();
				}
				ImGui::PushID(entry.id.c_str());
				PaintTileSource source{ MakeTileSource(ctx, entry.texture, entry.slice) };
				ImVec2 p0{ ImGui::GetCursorScreenPos() };
				ImGui::InvisibleButton("##Tile", { cell, cell });
				ImVec2 p1{ p0.x + cell, p0.y + cell };
				auto* dl{ ImGui::GetWindowDrawList() };
				dl->AddRectFilled(p0, p1, ImGui::GetColorU32(ImGuiCol_FrameBg), 3.0f);
				auto record_it{ std::ranges::find_if(records, [&](const auto& r) {
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
				bool tile_clicked{ ImGui::IsItemClicked(ImGuiMouseButton_Left) };
				bool tile_hovered{ ImGui::IsItemHovered() };
				if (tile_clicked) {
					RequestPrimaryInspector(ctx, EditorSelectionMode::Tiles);
					selected_tile_entry_id_ = entry.id;
					inspected_texture_		= entry.texture;
					inspected_slice_		= entry.slice;
					recipe_.source_kind		= PaintSourceKind::Single;
					tile_source_			= source;
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
					if (ImGui::MenuItem("Reimport Source Tileset")) {
						pending_reimport = entry.texture;
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
				DeleteTileLibraryEntry(ctx, *delete_id);
			}
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	// Item context menus take priority over the panel background menu. An item popup can
	// block hover detection when the right mouse button is released, so also check for
	// existing popups before opening the background menu.
	if (ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) &&
		!ImGui::IsAnyItemHovered() &&
		!ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId | ImGuiPopupFlags_AnyPopupLevel)) {
		ImGui::OpenPopup("TilesPanelContextMenu");
	}
	if (ImGui::BeginPopup("TilesPanelContextMenu")) {
		if (ImGui::MenuItem("Create Tile Group")) {
			create_group();
		}
		ImGui::Separator();
		if (ImGui::MenuItem("Add Tiles")) {
			add_tiles("Ungrouped");
		}
		ImGui::EndPopup();
	}

	if (!group_to_rename.empty()) {
		RenameResult result{ DrawRenameModal(
			group_rename_modal, "RenameTileGroup", "##GroupName",
			[&](std::string_view candidate) -> std::string {
				std::string name{ Trim(std::string{ candidate }) };
				if (name.empty() || name == "Ungrouped") {
					return "Enter a valid group name.";
				}
				if (name != group_to_rename && std::ranges::contains(TileGroupNames(), name)) {
					return "A group with that name already exists.";
				}
				return {};
			},
			[&](std::string_view candidate) {
				GroupSnapshot before{ capture_groups() };
				std::string name{ Trim(std::string{ candidate }) };
				for (auto& entry : tile_library_) {
					if (entry.group == group_to_rename) {
						entry.group = name;
					}
				}
				for (auto& entry : autotile_library_) {
					if (entry.group == group_to_rename) {
						entry.group = name;
					}
				}
				for (auto& current : tile_groups_) {
					if (current == group_to_rename) {
						current = name;
					}
				}
				for (auto& current : autotile_groups_) {
					if (current == group_to_rename) {
						current = name;
					}
				}
				std::ranges::sort(tile_groups_);
				std::ranges::sort(autotile_groups_);
				commit_groups("Rename Tile Group", std::move(before));
			},
			RenameModalOptions{ .title = "Rename Tile Group" }
		) };
		if (result != RenameResult::None) {
			group_to_rename.clear();
		}
	}

	if (pending_group_move.has_value() && !IsAutotileGroup(pending_group_move->second) &&
		!pending_group_move->first.starts_with("auto:")) {
		if (TileLibraryEntry* entry{ FindTileEntry(pending_group_move->first) };
			entry && entry->group != pending_group_move->second) {
			GroupSnapshot before{ capture_groups() };
			entry->group = pending_group_move->second;
			commit_groups("Move Tile to Group", std::move(before));
		}
	}
	if (pending_reimport) {
		TileImportSettings settings;
		settings.replace_texture		  = *pending_reimport;
		settings.create_group_from_source = false;
		if (auto it{ std::ranges::find(
				autotile_library_, *pending_reimport, &AutotileLibraryEntry::texture
			) };
			it != autotile_library_.end()) {
			settings.autotile		 = true;
			settings.autotile_format = it->format;
			settings.target_group	 = it->group;
			if (const TileSliceSettings* slicing{ FindSliceSettings(*pending_reimport) }) {
				settings.tile_width	 = slicing->tile_size.x;
				settings.tile_height = slicing->tile_size.y;
			}
		} else {
			settings.mode					 = TileImportMode::Tileset;
			settings.use_filename_dimensions = false;
			if (const TileSliceSettings* slicing{ FindSliceSettings(*pending_reimport) }) {
				settings.tile_width	 = slicing->tile_size.x;
				settings.tile_height = slicing->tile_size.y;
			}
			for (const auto& tile : tile_library_) {
				if (tile.texture == *pending_reimport) {
					settings.target_group = tile.group;
					break;
				}
			}
		}
		auto result{ ctx.editor.GetWindow().file.OpenFiles(TileImportOptions()) };
		if (result && result->has_value() && !result->value().empty()) {
			settings.path = result->value().front().string();
			int count{ ImportTiles(ctx, settings) };
			import_status_ = count > 0
							   ? "Reimported source tileset."
							   : "Reimport failed. Check source format, extension and dimensions.";
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

bool PaintEditor::DrawGeneratorSourceEditor(
	EditorContext& ctx, Entity generator, PaintGeneratorRecipe& recipe
) {
	if (!generator || !IsPaintGenerator(generator)) {
		return false;
	}

	EnsureProjectLibrary(ctx);

	bool targets_tilemap{ static_cast<bool>(PaintGenerator{ generator }.GetTargetTilemap()) };
	bool changed{ false };

	auto to_editor = [](
						 const std::optional<PaintGeneratorTileSource>& source
					 ) -> std::optional<PaintTileSource> {
		if (!source || !source->texture) {
			return std::nullopt;
		}
		return PaintTileSource{
			.texture			 = source->texture,
			.texture_coordinates = source->texture_coordinates,
			.pixel_size			 = source->pixel_size,
		};
	};
	auto to_generator = [](const std::optional<PaintTileSource>& source,
						   Origin origin) -> std::optional<PaintGeneratorTileSource> {
		if (!source || !*source) {
			return std::nullopt;
		}
		return PaintGeneratorTileSource{
			.texture			 = source->texture,
			.texture_coordinates = source->texture_coordinates,
			.pixel_size			 = source->pixel_size,
			.origin				 = origin,
		};
	};

	auto ensure_noise_threshold = [&](PaintGeneratorSourceKind previous_source) {
		if (!recipe.noise.thresholds.empty()) {
			return false;
		}

		PaintGeneratorNoiseThreshold initial{
			.minimum	 = 0.0f,
			.maximum	 = 1.0f,
			.enabled	 = true,
			.source_kind = PaintGeneratorSourceKind::Single,
			.origin		 = targets_tilemap ? recipe.tile_origin : recipe.entity_origin,
		};

		if (previous_source == PaintGeneratorSourceKind::WeightedSet) {
			if (targets_tilemap && !recipe.weighted_tiles.empty()) {
				initial.source_kind	   = PaintGeneratorSourceKind::WeightedSet;
				initial.weighted_tiles = recipe.weighted_tiles;
				for (auto& entry : initial.weighted_tiles) {
					entry.source.origin = initial.origin;
				}
			} else if (!targets_tilemap && !recipe.weighted_prefabs.empty()) {
				initial.source_kind		 = PaintGeneratorSourceKind::WeightedSet;
				initial.weighted_prefabs = recipe.weighted_prefabs;
			}
		}

		if (initial.source_kind == PaintGeneratorSourceKind::Single) {
			if (targets_tilemap) {
				initial.tile = recipe.tile;
				if (!initial.tile.has_value() &&
					previous_source == PaintGeneratorSourceKind::Autotile) {
					auto slot{ std::ranges::find_if(recipe.autotile_tiles, [](const auto& value) {
						return value.has_value();
					}) };
					if (slot != recipe.autotile_tiles.end()) {
						initial.tile = *slot;
					}
				}
				if (initial.tile.has_value()) {
					initial.tile->origin = initial.origin;
				}
			} else {
				initial.prefab = recipe.prefab;
			}
		}

		recipe.noise.thresholds.push_back(std::move(initial));
		return true;
	};

	int displayed_source{ targets_tilemap ? static_cast<int>(recipe.source_kind)
						  : recipe.source_kind == PaintGeneratorSourceKind::Noise
							  ? 3
							  : std::min(static_cast<int>(recipe.source_kind), 2) };
	const char* items{ targets_tilemap
						   ? "Single\0Weighted Set\0Checkerboard\0Autotile / Terrain\0Noise\0"
						   : "Single\0Weighted Set\0Checkerboard\0Noise\0" };
	if (inspector::DrawInspectorPropertyRow("Source", [&]() { return ImGui::Combo("##Source", &displayed_source, items, targets_tilemap ? 5 : 4); })) {
		PaintGeneratorSourceKind previous_source{ recipe.source_kind };
		recipe.source_kind =
			targets_tilemap			? static_cast<PaintGeneratorSourceKind>(displayed_source)
			: displayed_source == 3 ? PaintGeneratorSourceKind::Noise
									: static_cast<PaintGeneratorSourceKind>(displayed_source);

		if (recipe.source_kind == PaintGeneratorSourceKind::Noise) {
			if (previous_source != PaintGeneratorSourceKind::Noise) {
				// Thresholds are source specific generator state. Do not resurrect
				// unrelated/stale paint toolbar noise ranges when changing source type.
				recipe.noise.thresholds.clear();
			}
			ensure_noise_threshold(previous_source);
		}
		changed = true;
	}

	switch (recipe.source_kind) {
		case PaintGeneratorSourceKind::Single:
			if (targets_tilemap) {
				auto source{ to_editor(recipe.tile) };
				if (inspector::DrawInspectorPropertyRow("Tile", [&]() {
                    return DrawTileSourceCombo(ctx, "##GeneratorSourceTile", source);
                })) {
					recipe.tile = to_generator(source, recipe.tile_origin);
					changed		= true;
				}
			} else {
				auto source{ recipe.prefab };
				if (inspector::DrawInspectorPropertyRow("Prefab", [&]() {
                    return DrawPrefabSourceCombo(ctx, "##GeneratorSourcePrefab", source);
                })) {
					recipe.prefab = std::move(source);
					changed		  = true;
				}
			}
			break;

		case PaintGeneratorSourceKind::Checkerboard:
			if (targets_tilemap) {
				auto first{ to_editor(recipe.tile) };
				auto second{ to_editor(recipe.checker_tile) };
				if (inspector::DrawInspectorPropertyRow("First Tile", [&]() {
                    return DrawTileSourceCombo(ctx, "##GeneratorCheckerFirst", first);
                })) {
					recipe.tile = to_generator(first, recipe.tile_origin);
					changed		= true;
				}
				if (inspector::DrawInspectorPropertyRow("Second Tile", [&]() {
                    return DrawTileSourceCombo(ctx, "##GeneratorCheckerSecond", second);
                })) {
					recipe.checker_tile = to_generator(second, recipe.tile_origin);
					changed				= true;
				}
			} else {
				auto first{ recipe.prefab };
				auto second{ recipe.checker_prefab };
				if (inspector::DrawInspectorPropertyRow("First Prefab", [&]() {
                    return DrawPrefabSourceCombo(ctx, "##GeneratorCheckerFirstPrefab", first);
                })) {
					recipe.prefab = std::move(first);
					changed		  = true;
				}
				if (inspector::DrawInspectorPropertyRow("Second Prefab", [&]() {
                    return DrawPrefabSourceCombo(ctx, "##GeneratorCheckerSecondPrefab", second);
                })) {
					recipe.checker_prefab = std::move(second);
					changed				  = true;
				}
			}
			break;

		case PaintGeneratorSourceKind::Autotile:
			if (targets_tilemap) {
				TextureKey current{};
				for (const auto& slot : recipe.autotile_tiles) {
					if (slot && slot->texture) {
						current = slot->texture;
						break;
					}
				}

				auto imported{
					std::ranges::find(autotile_library_, current, &AutotileLibraryEntry::texture)
				};
				if (imported != autotile_library_.end()) {
					auto format{ static_cast<PaintGeneratorAutotileFormat>(imported->format) };
					if (recipe.autotile_format != format) {
						recipe.autotile_format = format;
						changed				   = true;
					}
					inspector::DrawInspectorPropertyRow("Layout", [&]() {
                        ImGui::TextDisabled("%s", AutotileFormatName(imported->format));
                        return false;
                    });
				}

				std::string preview{ imported != autotile_library_.end() && !imported->name.empty()
										 ? imported->name
									 : current ? TextureDisplayName(current)
											   : std::string{ "<none>" } };
				if (ImGui::BeginCombo("Tileset", preview.c_str())) {
					for (const auto& entry : autotile_library_) {
						ImGui::PushID(entry.texture.value.c_str());
						bool selected{ entry.texture == current };
						std::string label{ entry.name.empty() ? TextureDisplayName(entry.texture)
															  : entry.name };
						label += " (";
						label += AutotileFormatName(entry.format);
						label += ")";
						if (ImGui::Selectable(label.c_str(), selected)) {
							if (auto* rules{
									EnsureAutotileRuleSet(ctx, entry.texture, entry.format) }) {
								recipe.autotile_format =
									static_cast<PaintGeneratorAutotileFormat>(entry.format);
								recipe.autotile_tiles.assign(rules->tiles.size(), std::nullopt);
								for (std::size_t i{}; i < rules->tiles.size(); ++i) {
									if (!rules->tiles[i]) {
										continue;
									}
									recipe.autotile_tiles[i] = PaintGeneratorTileSource{
										.texture			 = rules->tiles[i]->texture,
										.texture_coordinates = rules->tiles[i]->texture_coordinates,
										.pixel_size			 = rules->tiles[i]->pixel_size,
										.origin				 = Origin::TopLeft,
									};
								}
								changed = true;
							}
						}
						if (selected) {
							ImGui::SetItemDefaultFocus();
						}
						ImGui::PopID();
					}
					ImGui::EndCombo();
				}
			}
			break;

		case PaintGeneratorSourceKind::WeightedSet:
			ImGui::TextDisabled(
				"Weighted members are captured by this generator. Switch source type to replace it."
			);
			break;

		case PaintGeneratorSourceKind::Noise: {
			changed |= ensure_noise_threshold(
				!recipe.weighted_tiles.empty() || !recipe.weighted_prefabs.empty()
					? PaintGeneratorSourceKind::WeightedSet
					: PaintGeneratorSourceKind::Single
			);

			ImGui::PushID("GeneratorNoiseSettings");
			ImGui::SeparatorText("Noise Source");

			int type_index{};
			if (recipe.noise.type == NoiseType::Simplex) {
				type_index = 1;
			} else if (recipe.noise.type == NoiseType::Value) {
				type_index = 2;
			}
			if (inspector::DrawInspectorPropertyRow("Type", [&]() { return ImGui::Combo("##Type", &type_index, "Perlin\0Simplex\0Value\0"); })) {
				recipe.noise.type = type_index == 0 ? NoiseType::Perlin
								  : type_index == 1 ? NoiseType::Simplex
													: NoiseType::Value;
				changed			  = true;
			}
			changed |= inspector::DrawInspectorPropertyRow("Seed", [&]() { return ImGui::DragInt("##Seed", &recipe.noise.seed, 1.0f); });
			changed |= inspector::DrawInspectorPropertyRow("Frequency", [&]() { return ImGui::DragFloat(
				"##Frequency", &recipe.noise.frequency, 0.001f, 0.0001f, 1.0f, "%.4f",
				ImGuiSliderFlags_AlwaysClamp
			); });
			changed |= inspector::DrawInspectorPropertyRow("Octaves", [&]() { return ImGui::DragInt("##Octaves", &recipe.noise.octaves, 0.2f, 1, 12); });
			changed |= inspector::DrawInspectorPropertyRow("Lacunarity", [&]() { return ImGui::DragFloat(
				"##Lacunarity", &recipe.noise.lacunarity, 0.02f, 1.0f, 8.0f, "%.2f",
				ImGuiSliderFlags_AlwaysClamp
			); });
			changed |=
				inspector::DrawInspectorPropertyRow("Persistence", [&]() { return ImGui::SliderFloat("##Persistence", &recipe.noise.persistence, 0.0f, 1.0f, "%.2f"); });

			float offset[2]{ recipe.noise.offset.x, recipe.noise.offset.y };
			if (inspector::DrawInspectorPropertyRow("Noise Offset", [&]() { return ImGui::DragFloat2("##Noise Offset", offset, 0.25f); })) {
				recipe.noise.offset = { offset[0], offset[1] };
				changed				= true;
			}

			changed |= inspector::DrawInspectorPropertyRow("Show Noise Preview", [&]() { return ImGui::Checkbox("##Show Noise Preview", &recipe.show_noise_preview); });
			if (recipe.show_noise_preview) {
				changed |= inspector::DrawInspectorPropertyRow("Noise Preview Opacity", [&]() { return ImGui::SliderFloat(
					"##Noise Preview Opacity", &recipe.noise_preview_alpha, 0.0f, 1.0f, "%.2f"
				); });
			}

			ImGui::SeparatorText("Noise Thresholds");
			auto& thresholds{ recipe.noise.thresholds };

			auto normalize = [&] {
				std::ranges::sort(thresholds, {}, [](const PaintGeneratorNoiseThreshold& region) {
					return region.minimum;
				});
				thresholds.front().minimum = 0.0f;
				for (std::size_t i{}; i < thresholds.size(); ++i) {
					auto& region{ thresholds[i] };
					region.minimum = std::clamp(region.minimum, 0.0f, 1.0f);
					region.maximum = std::clamp(region.maximum, region.minimum, 1.0f);
					if (i > 0) {
						region.minimum = thresholds[i - 1].maximum;
					}
				}
				thresholds.back().maximum = 1.0f;
			};
			auto split = [&](float value) {
				value = std::clamp(value, 0.01f, 0.99f);
				normalize();
				for (std::size_t i{}; i < thresholds.size(); ++i) {
					auto& region{ thresholds[i] };
					if (value <= region.minimum + 0.002f || value >= region.maximum - 0.002f) {
						continue;
					}
					PaintGeneratorNoiseThreshold right{ region };
					right.enabled  = true;
					right.minimum  = value;
					region.maximum = value;
					thresholds.insert(
						thresholds.begin() + static_cast<std::ptrdiff_t>(i + 1), std::move(right)
					);
					changed = true;
					return;
				}
			};
			auto remove_boundary = [&](int boundary) {
				if (boundary < 0 || boundary + 1 >= static_cast<int>(thresholds.size())) {
					return;
				}
				thresholds[static_cast<std::size_t>(boundary)].maximum =
					thresholds[static_cast<std::size_t>(boundary + 1)].maximum;
				thresholds.erase(thresholds.begin() + boundary + 1);
				normalize();
				changed = true;
			};
			auto source_label = [&](const PaintGeneratorNoiseThreshold& region) {
				if (!region.enabled) {
					return std::string{ "None" };
				}
				if (region.source_kind == PaintGeneratorSourceKind::WeightedSet) {
					std::size_t count{ targets_tilemap ? region.weighted_tiles.size()
													   : region.weighted_prefabs.size() };
					return std::string{ "Weighted (" } + std::to_string(count) + ")";
				}
				if (targets_tilemap) {
					return region.tile.has_value() && region.tile->texture
							 ? region.tile->texture.value
							 : std::string{ "None" };
				}
				return region.prefab.has_value() ? PrefabDisplayName(*region.prefab)
												 : std::string{ "None" };
			};
			auto truncated = [](std::string label, float available) {
				if (available < 28.0f) {
					return std::string{};
				}
				int maximum{ std::max(3, static_cast<int>(available / 7.0f)) };
				if (static_cast<int>(label.size()) <= maximum) {
					return label;
				}
				return label.substr(0, static_cast<std::size_t>(std::max(1, maximum - 3))) + "...";
			};

			normalize();
			float width{ std::max(260.0f, ImGui::GetContentRegionAvail().x) };
			float height{ 76.0f };
			ImVec2 p0{ ImGui::GetCursorScreenPos() };
			ImGui::InvisibleButton(
				"##generator_noise_threshold_gradient", { width, height },
				ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight
			);
			ImVec2 mouse{ ImGui::GetIO().MousePos };
			ImDrawList* draw{ ImGui::GetWindowDrawList() };
			float bar_y0{ p0.y + 25.0f };
			float bar_y1{ p0.y + 47.0f };

			for (int i{}; i < 96; ++i) {
				float a{ static_cast<float>(i) / 96.0f };
				float b{ static_cast<float>(i + 1) / 96.0f };
				float value{ (a + b) * 0.5f };
				draw->AddRectFilled(
					{ p0.x + a * width, bar_y0 }, { p0.x + b * width + 1.0f, bar_y1 },
					ImGui::GetColorU32(ImVec4(value, value, value, 1.0f))
				);
			}
			draw->AddRect(
				{ p0.x, bar_y0 }, { p0.x + width, bar_y1 }, ImGui::GetColorU32(ImGuiCol_Border)
			);

			for (std::size_t i{}; i < thresholds.size(); ++i) {
				const auto& region{ thresholds[i] };
				float x0{ p0.x + region.minimum * width };
				float x1{ p0.x + region.maximum * width };
				if (region.enabled) {
					draw->AddRect(
						{ x0 + 1.0f, bar_y0 + 1.0f }, { x1 - 1.0f, bar_y1 - 1.0f },
						ImGui::GetColorU32(ImVec4(0.95f, 0.78f, 0.26f, 0.95f)), 0.0f, 0, 2.0f
					);
				}
				std::string label{
					truncated(source_label(region), std::max(0.0f, x1 - x0 - 4.0f))
				};
				if (!label.empty()) {
					ImVec2 size{ ImGui::CalcTextSize(label.c_str()) };
					float x{ std::clamp((x0 + x1 - size.x) * 0.5f, p0.x, p0.x + width - size.x) };
					float y{ (i % 2 == 0) ? p0.y + 3.0f : bar_y1 + 6.0f };
					draw->AddText(
						{ x, y },
						ImGui::GetColorU32(region.enabled ? ImGuiCol_Text : ImGuiCol_TextDisabled),
						label.c_str()
					);
				}
			}

			for (int i{}; i + 1 < static_cast<int>(thresholds.size()); ++i) {
				float x{ p0.x + thresholds[static_cast<std::size_t>(i)].maximum * width };
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
			ImGuiID widget_id{ ImGui::GetID("##generator_noise_threshold_gradient") };
			auto nearest_boundary = [&] {
				int nearest{ -1 };
				float best{ 9.0f };
				for (int i{}; i + 1 < static_cast<int>(thresholds.size()); ++i) {
					float x{ p0.x + thresholds[static_cast<std::size_t>(i)].maximum * width };
					float distance{ std::abs(mouse.x - x) };
					if (distance < best) {
						best	= distance;
						nearest = i;
					}
				}
				return nearest;
			};

			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				int nearest{ nearest_boundary() };
				if (nearest >= 0) {
					dragging_id		  = widget_id;
					dragging_boundary = nearest;
				} else {
					split(std::clamp((mouse.x - p0.x) / width, 0.01f, 0.99f));
				}
			}
			if (dragging_id == widget_id && dragging_boundary >= 0 &&
				ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				float value{ std::clamp((mouse.x - p0.x) / width, 0.0f, 1.0f) };
				float low{ thresholds[static_cast<std::size_t>(dragging_boundary)].minimum +
						   0.005f };
				float high{ thresholds[static_cast<std::size_t>(dragging_boundary + 1)].maximum -
							0.005f };
				float boundary{ std::clamp(value, low, high) };
				auto& left{ thresholds[static_cast<std::size_t>(dragging_boundary)] };
				auto& right{ thresholds[static_cast<std::size_t>(dragging_boundary + 1)] };
				if (left.maximum != boundary || right.minimum != boundary) {
					left.maximum  = boundary;
					right.minimum = boundary;
					changed		  = true;
				}
			}
			if (dragging_id == widget_id && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
				dragging_id		  = 0;
				dragging_boundary = -1;
			}
			if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				int nearest{ nearest_boundary() };
				if (nearest >= 0) {
					remove_boundary(nearest);
				}
			}
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip(
					"Left click empty gradient space: split a range.\n"
					"Left drag a divider: move the threshold.\n"
					"Right click a divider: remove it and merge its neighboring ranges."
				);
			}

			int remove_region{ -1 };
			for (int i{}; i < static_cast<int>(thresholds.size()); ++i) {
				auto& region{ thresholds[static_cast<std::size_t>(i)] };
				ImGui::PushID(i);

				bool enabled{ region.enabled };
				if (ImGui::Checkbox("##GeneratorNoiseRangeEnabled", &enabled)) {
					region.enabled = enabled;
					changed		   = true;
				}
				ImGui::SameLine();
				ImGui::TextDisabled("%.3f - %.3f", region.minimum, region.maximum);

				ImGui::SameLine();
				int kind{ !region.enabled												? 0
						  : region.source_kind == PaintGeneratorSourceKind::WeightedSet ? 2
																						: 1 };
				ImGui::SetNextItemWidth(112.0f);
				if (ImGui::Combo(
						"##GeneratorNoiseRangeKind", &kind, "None\0Single\0Weighted Set\0"
					)) {
					region.enabled	   = kind != 0;
					region.source_kind = kind == 2 ? PaintGeneratorSourceKind::WeightedSet
												   : PaintGeneratorSourceKind::Single;
					changed			   = true;
				}

				if (region.enabled) {
					ImGui::SameLine();
					ImGui::SetNextItemWidth(220.0f);

					if (targets_tilemap) {
						if (region.source_kind == PaintGeneratorSourceKind::Single) {
							auto source{ to_editor(region.tile) };
							if (DrawTileSourceCombo(ctx, "##GeneratorNoiseRangeTile", source)) {
								region.tile = to_generator(source, region.origin);
								changed		= true;
							}
						} else {
							std::string preview{
								region.weighted_tiles.empty()
									? std::string{ "<none>" }
									: std::string{ "Captured (" } +
										  std::to_string(region.weighted_tiles.size()) + " tiles)"
							};
							if (ImGui::BeginCombo(
									"##GeneratorNoiseRangeTileSet", preview.c_str()
								)) {
								for (const auto& candidate : weighted_tile_sets_) {
									if (ImGui::Selectable(candidate.name.c_str(), false)) {
										region.weighted_tiles.clear();
										for (const auto& entry : candidate.entries) {
											if (!entry.source) {
												continue;
											}
											region.weighted_tiles.push_back(
												PaintGeneratorWeightedTileEntry{
													.source =
														PaintGeneratorTileSource{
															.texture = entry.source.texture,
															.texture_coordinates =
																entry.source.texture_coordinates,
															.pixel_size = entry.source.pixel_size,
															.origin		= region.origin,
														},
													.weight = entry.weight,
												}
											);
										}
										changed = true;
									}
								}
								ImGui::EndCombo();
							}
						}
					} else {
						if (region.source_kind == PaintGeneratorSourceKind::Single) {
							auto source{ region.prefab };
							if (DrawPrefabSourceCombo(ctx, "##GeneratorNoiseRangePrefab", source)) {
								region.prefab = std::move(source);
								changed		  = true;
							}
						} else {
							std::string preview{
								region.weighted_prefabs.empty()
									? std::string{ "<none>" }
									: std::string{ "Captured (" } +
										  std::to_string(region.weighted_prefabs.size()) +
										  " prefabs)"
							};
							if (ImGui::BeginCombo(
									"##GeneratorNoiseRangePrefabSet", preview.c_str()
								)) {
								for (const auto& candidate : weighted_prefab_sets_) {
									if (ImGui::Selectable(candidate.name.c_str(), false)) {
										region.weighted_prefabs.clear();
										for (const auto& entry : candidate.entries) {
											if (!entry.prefab) {
												continue;
											}
											region.weighted_prefabs.push_back(
												PaintGeneratorWeightedPrefabEntry{
													.prefab = entry.prefab,
													.weight = entry.weight,
												}
											);
										}
										changed = true;
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
					Origin old_origin{ region.origin };
					if (DrawOriginCombo("##GeneratorNoiseRangeOrigin", region.origin)) {
						if (region.tile.has_value()) {
							region.tile->origin = region.origin;
						}
						for (auto& entry : region.weighted_tiles) {
							entry.source.origin = region.origin;
						}
						changed = true;
					} else if (old_origin != region.origin) {
						changed = true;
					}
				}

				if (thresholds.size() > 1) {
					ImGui::SameLine();
					if (ImGui::SmallButton("x")) {
						remove_region = i;
					}
				}
				ImGui::PopID();
			}

			if (remove_region >= 0 && thresholds.size() > 1) {
				int boundary{
					std::clamp(remove_region, 0, static_cast<int>(thresholds.size()) - 2)
				};
				thresholds[static_cast<std::size_t>(boundary)].maximum =
					thresholds[static_cast<std::size_t>(boundary + 1)].maximum;
				thresholds.erase(thresholds.begin() + boundary + 1);
				normalize();
				changed = true;
			}

			ImGui::PopID();
			break;
		}
	}

	return changed;
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

	bool tile_mode{ static_cast<bool>(generator.GetTargetTilemap()) };

	SerializedEntity generator_snapshot{ SerializeEntity(entity) };
	std::optional<UUID> generator_parent{ HasParent(entity)
											  ? std::optional<UUID>{ GetParent(entity).Get<UUID>() }
											  : std::nullopt };
	std::optional<Transform> generator_world_transform{
		entity.Has<Transform>() ? std::optional<Transform>{ GetWorldTransform(entity) }
								: std::nullopt
	};
	V2_float entity_offset{ GetWorldPosition(generator) };
	V2_float origin{ data.grid_offset + entity_offset };
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
		int dx{ std::abs(b.x - a.x) };
		int dy{ -std::abs(b.y - a.y) };
		int sx{ a.x < b.x ? 1 : -1 };
		int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};

		for (;;) {
			if (step % std::max(1, spacing) == 0) {
				int half{ std::max(1, thickness) / 2 };
				for (int y{ -half }; y <= half; ++y) {
					for (int x{ -half }; x <= half; ++x) {
						result.emplace_back(a + V2_int{ x, y });
					}
				}
			}

			if (a == b) {
				break;
			}
			int e2{ 2 * error };
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
			V2_int a{ to_cell(data.start + entity_offset) };
			V2_int b{ to_cell(data.end + entity_offset) };
			int min_x{ std::min(a.x, b.x) };
			int max_x{ std::max(a.x, b.x) };
			int min_y{ std::min(a.y, b.y) };
			int max_y{ std::max(a.y, b.y) };

			for (int y{ min_y }; y <= max_y; ++y) {
				for (int x{ min_x }; x <= max_x; ++x) {
					int left{ x - min_x };
					int right{ max_x - x };
					int top{ y - min_y };
					int bottom{ max_y - y };
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
							int thickness{ std::max(1, data.area_thickness) };
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
				V2_int center{ to_cell(point.position + entity_offset) };
				int diameter{ std::max(1, point.diameter) };
				int low{ -(diameter / 2) };
				int high{ low + diameter - 1 };
				float radius{ static_cast<float>(diameter) * 0.5f };

				for (int y{ low }; y <= high; ++y) {
					for (int x{ low }; x <= high; ++x) {
						if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
							float offset{ BrushCircleCellOffset(diameter) };
							float dx{ static_cast<float>(x) + offset };
							float dy{ static_cast<float>(y) + offset };
							if (std::sqrt(dx * dx + dy * dy) > radius) {
								continue;
							}
						}
						AddGeneratorBrushCell(
							data.recipe, center + V2_int{ x, y },
							[&](V2_int cell) { cells.insert(cell); }
						);
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

		float noise_value{ data.recipe.source_kind == PaintGeneratorSourceKind::Noise
							   ? FractalNoise01(
									 world_center, data.recipe.noise.type, data.recipe.noise.seed,
									 data.recipe.noise.frequency, data.recipe.noise.octaves,
									 data.recipe.noise.lacunarity, data.recipe.noise.persistence,
									 data.recipe.noise.offset
								 )
							   : 0.0f };

		switch (data.recipe.source_kind) {
			case PaintGeneratorSourceKind::Single:
				result.tile	  = data.recipe.tile;
				result.prefab = data.recipe.prefab;
				break;

			case PaintGeneratorSourceKind::WeightedSet:
				if (tile_mode) {
					auto i{ ChooseWeightedIndex(
						data.recipe.weighted_tiles, cell,
						[](const auto& entry) { return entry.weight; }
					) };
					if (i) {
						result.tile = data.recipe.weighted_tiles[*i].source;
					}
				} else {
					auto i{ ChooseWeightedIndex(
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
				int index{ AutotileIndex(
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
					float lo{ std::min(region.minimum, region.maximum) };
					float hi{ std::max(region.minimum, region.maximum) };
					if (noise_value < lo ||
						(noise_value >= hi && !(hi >= 0.99999f && noise_value <= 1.0f))) {
						continue;
					}

					result.entity_origin = region.origin;
					if (region.source_kind == PaintGeneratorSourceKind::WeightedSet) {
						if (tile_mode) {
							auto i{ ChooseWeightedIndex(
								region.weighted_tiles, cell,
								[](const auto& entry) { return entry.weight; }
							) };
							if (i) {
								result.tile = region.weighted_tiles[*i].source;
							}
						} else {
							auto i{ ChooseWeightedIndex(
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

	if (tile_mode) {
		Tilemap map{ generator.GetTargetTilemap() };
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

			V2_float mn{ cell_world(cell) };
			V2_float center{ mn + data.grid_size * 0.5f };
			auto source{ resolve_source(cell, center) };
			if (!source.tile.has_value()) {
				continue;
			}

			const auto& tile{ *source.tile };
			V2_float visual_size{ tile.pixel_size.IsPositive() ? V2_float{ tile.pixel_size }
															   : data.grid_size };
			if (data.recipe.tile_placement == PaintGeneratorTilePlacementMode::Tile) {
				V2_int footprint{
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
				V2_int lattice_origin{ to_cell(data.start + entity_offset) };
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

			V2_float center{ cell_world(cell) + data.grid_size * 0.5f };
			auto source{ resolve_source(cell, center) };
			if (!source.prefab.has_value()) {
				continue;
			}

			PrefabKey prefab_key{ *source.prefab };

			bool occupied{};
			for (Entity root : SceneRootEntities(scene)) {
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

			if (!assets.EnsurePrefabResident(prefab_key)) {
				continue;
			}

			V2_float world{ cell_world(cell) + GetOffset(source.entity_origin, data.grid_size) };

			if (data.recipe.min_spacing > 0.0f) {
				bool too_close{};
				for (Entity root : SceneRootEntities(scene)) {
					if (!root || root == entity || !root.Has<Transform>() ||
						IsPaintGenerator(root) || IsProtectedSceneEntity(scene, root)) {
						continue;
					}
					V2_float delta{ GetWorldPosition(root) - world };
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

			Transform transform{ GetWorldTransform(created) };
			transform.position = world;
			SetWorldTransform(created, transform);

			float selector{ Hash01(cell) };
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
				}
			);
		}
	}

	bool baked_anything{ (tilemap_before.has_value() && tilemap_after.has_value() &&
						  json{ *tilemap_before } != json{ *tilemap_after }) ||
						 !created_entities.empty() };
	if (!baked_anything) {
		return;
	}

	UUID generator_uuid{ entity.Get<UUID>() };
	entity.Destroy();
	scene.Refresh();

	if (active_brush_generator_.has_value() && *active_brush_generator_ == generator_uuid) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		active_generator_created_tilemap_.reset();
		active_generator_undo_cursor_.reset();
		next_active_brush_stroke_id_ = 1;
	}

	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);

	Scene* scene_ptr{ &scene };

	auto restore_generator = [scene_ptr, generator_snapshot, generator_parent,
							  generator_world_transform]() {
		if (generator_snapshot.uuid.has_value() && scene_ptr->GetEntity(*generator_snapshot.uuid)) {
			return;
		}
		Entity restored{ RestoreEntityTree(*scene_ptr, generator_snapshot) };
		scene_ptr->Refresh();
		if (restored && generator_parent.has_value()) {
			if (Entity parent{ scene_ptr->GetEntity(*generator_parent) };
				parent && IsTilemap(parent)) {
				SetParent(restored, parent);
				if (generator_world_transform.has_value()) {
					SetWorldTransform(restored, *generator_world_transform);
				}
			}
		}
	};

	auto remove_generator = [scene_ptr, generator_uuid]() {
		if (Entity gen{ scene_ptr->GetEntity(generator_uuid) }) {
			gen.Destroy();
			scene_ptr->Refresh();
		}
	};

	if (tilemap_uuid.has_value() && tilemap_before.has_value() && tilemap_after.has_value()) {
		UUID map_uuid{ *tilemap_uuid };
		auto before{ *tilemap_before };
		auto after{ *tilemap_after };

		PushPaintUndo(
			ctx, scene, "Bake Generator",
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
				RestoreEntityTree(*scene_ptr, item.entity);
			}
			scene_ptr->Refresh();
		};

		PushPaintUndo(
			ctx, scene, "Bake Generator",
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

void PaintEditor::DrawTileInspector(EditorContext& ctx) {
	EnsureProjectLibrary(ctx);
	if (!inspected_texture_.has_value()) {
		ImGui::TextDisabled("Select a tile in the Tiles tab.");
		return;
	}

	TextureKey texture{ *inspected_texture_ };
	const auto& settings{ GetSliceSettings(ctx, texture) };
	const TileLibraryEntry* selected{ FindTileEntry(selected_tile_entry_id_) };
	if (selected) {
		ImGui::Text("%s", selected->name.c_str());
	}
	ImGui::TextDisabled("%s", texture.value.c_str());
	ImGui::Separator();

	if (ImGui::BeginTable(
			"##TileSliceSettings", 2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings
		)) {
		ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 72.0f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted("Tile Size");

		ImGui::TableSetColumnIndex(1);
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%d x %d", settings.tile_size.x, settings.tile_size.y);

		ImGui::EndTable();
	}

	ImGui::Text("Selected slice: %d, %d", inspected_slice_.x, inspected_slice_.y);
}

V2_float PaintEditor::ActiveGridSize(const Scene& scene) const {
	if (IsTileMode(scene)) {
		if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			return map.GetData().cell_size;
		}
	}
	return entity_grid_size_;
}

V2_float PaintEditor::ActiveGridOrigin(const Scene& scene) const {
	if (IsTileMode(scene)) {
		if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			return GetWorldPosition(map);
		}
	}
	return entity_grid_offset_;
}

V2_int PaintEditor::WorldToActiveCell(const Scene& scene, V2_float world) const {
	V2_float size{ ActiveGridSize(scene) };
	V2_float origin{ ActiveGridOrigin(scene) };
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
	int diameter{ std::max(1, brush_diameter_) };
	int low{ -(diameter / 2) };
	int high{ low + diameter - 1 };
	float radius{ static_cast<float>(diameter) * 0.5f };
	float offset{ BrushCircleCellOffset(diameter) };

	for (int y{ low }; y <= high; ++y) {
		for (int x{ low }; x <= high; ++x) {
			if (brush_shape_ == PaintBrushShape::Circle) {
				float dx{ static_cast<float>(x) + offset };
				float dy{ static_cast<float>(y) + offset };
				if (std::sqrt(dx * dx + dy * dy) > radius) {
					continue;
				}
			}
			cells.emplace_back(center + V2_int{ x, y });
		}
	}
	// Classic 15 uses the occupancy of four grid vertices to choose one display tile.
	// A regular N-cell brush needs (N + 1) x (N + 1) occupied vertices so the
	// visible result is N + 2 cells wide: one full tile at the click for diameter
	// one, with the eight edge and corner variants surrounding it.
	if (recipe_.source_kind == PaintSourceKind::Autotile &&
		recipe_.autotile_format == PaintAutotileFormat::Classic15 &&
		recipe_.operation != PaintBrushOperation::ExclusionMask) {
		std::vector<V2_int> corners;
		corners.reserve(cells.size() * 4);
		for (V2_int cell : cells) {
			corners.push_back(cell);
			corners.push_back(cell + V2_int{ 1, 0 });
			corners.push_back(cell + V2_int{ 0, 1 });
			corners.push_back(cell + V2_int{ 1, 1 });
		}

		std::ranges::sort(corners, [](V2_int a, V2_int b) {
			return a.y == b.y ? a.x < b.x : a.y < b.y;
		});
		corners.erase(std::unique(corners.begin(), corners.end()), corners.end());
		return corners;
	}

	return cells;
}

std::vector<V2_int> PaintEditor::SelectionBrushCells(V2_int center) const {
	std::vector<V2_int> cells;
	int diameter{ std::max(1, selection_diameter_) };
	int low{ -(diameter / 2) };
	int high{ low + diameter - 1 };
	float radius{ static_cast<float>(diameter) * 0.5f };
	float offset{ BrushCircleCellOffset(diameter) };
	for (int y{ low }; y <= high; ++y) {
		for (int x{ low }; x <= high; ++x) {
			if (selection_brush_shape_ == PaintBrushShape::Circle) {
				float dx{ static_cast<float>(x) + offset };
				float dy{ static_cast<float>(y) + offset };
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
	int dx{ std::abs(b.x - a.x) };
	int dy{ -std::abs(b.y - a.y) };
	int sx{ a.x < b.x ? 1 : -1 };
	int sy{ a.y < b.y ? 1 : -1 };
	int error{ dx + dy };
	int step{};

	for (;;) {
		if (step % std::max(1, line_spacing_) == 0) {
			int half{ std::max(1, line_thickness_) / 2 };
			for (int oy{ -half }; oy <= half; ++oy) {
				for (int ox{ -half }; ox <= half; ++ox) {
					cells.emplace_back(a + V2_int{ ox, oy });
				}
			}
		}
		if (a == b) {
			break;
		}
		int e2{ 2 * error };
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
	int min_x{ std::min(a.x, b.x) };
	int max_x{ std::max(a.x, b.x) };
	int min_y{ std::min(a.y, b.y) };
	int max_y{ std::max(a.y, b.y) };
	std::vector<V2_int> cells;
	for (int y{ min_y }; y <= max_y; ++y) {
		for (int x{ min_x }; x <= max_x; ++x) {
			int left{ x - min_x };
			int right{ max_x - x };
			int top{ y - min_y };
			int bottom{ max_y - y };
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
					int thickness{ std::max(1, area_thickness_) };
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
	float selector{ Hash01(cell) };
	if (recipe_.coverage == PaintCoverageMode::RandomDensity) {
		return selector <= recipe_.density;
	}
	V2_float center{
		(static_cast<float>(first.x + last.x) + 1.0f) * 0.5f,
		(static_cast<float>(first.y + last.y) + 1.0f) * 0.5f,
	};
	V2_float p{ static_cast<float>(cell.x) + 0.5f, static_cast<float>(cell.y) + 0.5f };
	V2_float span{
		std::max(1.0f, static_cast<float>(last.x - first.x + 1)) * 0.5f,
		std::max(1.0f, static_cast<float>(last.y - first.y + 1)) * 0.5f,
	};
	float nx{ (p.x - center.x) / span.x };
	float ny{ (p.y - center.y) / span.y };
	float normalized{ std::min(1.0f, std::sqrt(nx * nx + ny * ny)) };
	float inner{ std::clamp(recipe_.radial_inner, 0.0f, 1.0f) };
	float outer{ std::max(inner + 0.001f, std::clamp(recipe_.radial_outer, 0.0f, 1.0f)) };
	float t{ std::clamp((normalized - inner) / (outer - inner), 0.0f, 1.0f) };
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
	int x0{ static_cast<int>(std::floor(x)) };
	int y0{ static_cast<int>(std::floor(y)) };
	float tx{ x - static_cast<float>(x0) };
	float ty{ y - static_cast<float>(y0) };
	float u{ NoiseFade(tx) };
	float v{ NoiseFade(ty) };
	float a{ NoiseGrad(NoiseHash2(x0, y0, seed), tx, ty) };
	float b{ NoiseGrad(NoiseHash2(x0 + 1, y0, seed), tx - 1.0f, ty) };
	float c{ NoiseGrad(NoiseHash2(x0, y0 + 1, seed), tx, ty - 1.0f) };
	float d{ NoiseGrad(NoiseHash2(x0 + 1, y0 + 1, seed), tx - 1.0f, ty - 1.0f) };
	float ab{ a + (b - a) * u };
	float cd{ c + (d - c) * u };
	return std::clamp((ab + (cd - ab) * v) * 0.5f + 0.5f, 0.0f, 1.0f);
}

[[nodiscard]] float Value01(float x, float y, std::uint32_t seed) {
	int x0{ static_cast<int>(std::floor(x)) };
	int y0{ static_cast<int>(std::floor(y)) };
	float tx{ x - static_cast<float>(x0) };
	float ty{ y - static_cast<float>(y0) };
	float u{ NoiseFade(tx) };
	float v{ NoiseFade(ty) };
	auto value = [&](int xx, int yy) {
		return static_cast<float>(NoiseHash2(xx, yy, seed) & 0x00ffffffu) /
			   static_cast<float>(0x00ffffffu);
	};
	float ab{ value(x0, y0) + (value(x0 + 1, y0) - value(x0, y0)) * u };
	float cd{ value(x0, y0 + 1) + (value(x0 + 1, y0 + 1) - value(x0, y0 + 1)) * u };
	return std::clamp(ab + (cd - ab) * v, 0.0f, 1.0f);
}

[[nodiscard]] float Simplex01(float xin, float yin, std::uint32_t seed) {
	constexpr float f2{ 0.3660254037844386f };
	constexpr float g2{ 0.2113248654051871f };
	float s{ (xin + yin) * f2 };
	int i{ static_cast<int>(std::floor(xin + s)) };
	int j{ static_cast<int>(std::floor(yin + s)) };
	float t{ static_cast<float>(i + j) * g2 };
	float x0{ xin - (static_cast<float>(i) - t) };
	float y0{ yin - (static_cast<float>(j) - t) };
	int i1{ x0 > y0 ? 1 : 0 };
	int j1{ x0 > y0 ? 0 : 1 };
	float x1{ x0 - static_cast<float>(i1) + g2 };
	float y1{ y0 - static_cast<float>(j1) + g2 };
	float x2{ x0 - 1.0f + 2.0f * g2 };
	float y2{ y0 - 1.0f + 2.0f * g2 };
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
	float value{ NoiseValue(world, recipe_.noise) };
	for (const auto& region : recipe_.noise.thresholds) {
		if (!region.enabled) {
			continue;
		}
		float lo{ std::min(region.minimum, region.maximum) };
		float hi{ std::max(region.minimum, region.maximum) };
		if (value >= lo && (value < hi || (hi >= 0.99999f && value <= 1.0f))) {
			return &region;
		}
	}
	return nullptr;
}

PaintGeneratorRecipe PaintEditor::CaptureGeneratorRecipe(bool tile_mode) const {
	PaintGeneratorRecipe result;
	result.source_kind = static_cast<PaintGeneratorSourceKind>(recipe_.source_kind);
	result.coverage	   = static_cast<PaintGeneratorCoverageMode>(recipe_.coverage);
	if (tile_mode && tile_source_) {
		result.tile = PaintGeneratorTileSource{
			.texture			 = tile_source_.texture,
			.texture_coordinates = tile_source_.texture_coordinates,
			.pixel_size			 = tile_source_.pixel_size,
			.origin				 = recipe_.tile_origin,
		};
	} else if (!tile_mode && prefab_source_) {
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
	if (const auto* rules{
			FindAutotileRuleSet(recipe_.autotile_texture, recipe_.autotile_format) }) {
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
	result.avoid_exclusion_mask	 = true;
	result.link_prefab_instances = recipe_.link_prefab_instances;
	result.random_rotation		 = recipe_.random_rotation;
	result.rotation_min			 = recipe_.rotation_min;
	result.rotation_max			 = recipe_.rotation_max;
	result.random_scale			 = recipe_.random_scale;
	result.scale_min			 = recipe_.scale_min;
	result.scale_max			 = recipe_.scale_max;
	result.show_noise_preview	 = recipe_.show_noise_preview;
	result.noise_preview_alpha	 = recipe_.noise_preview_alpha;
	result.noise.type			 = recipe_.noise.type;
	result.noise.seed			 = recipe_.noise.seed;
	result.noise.frequency		 = recipe_.noise.frequency;
	result.noise.octaves		 = recipe_.noise.octaves;
	result.noise.lacunarity		 = recipe_.noise.lacunarity;
	result.noise.persistence	 = recipe_.noise.persistence;
	result.noise.offset			 = recipe_.noise.offset;
	if (recipe_.source_kind == PaintSourceKind::Noise) {
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
	}
	return result;
}

void PaintEditor::DrawTilemaps(
	EditorContext& ctx, Scene& scene, ImDrawList* draw, Viewport presentation_viewport,
	const FrameContext& frame
) const {
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };

	ImVec2 viewport_min{ ToImGui(presentation_viewport.position) };
	ImVec2 viewport_max{ ToImGui(presentation_viewport.position + presentation_viewport.size) };

	{
		for (Entity entity : scene.Entities()) {
			if (!IsTilemap(entity)) {
				continue;
			}

			Tilemap tilemap{ entity };
			const auto& data{ tilemap.GetData() };

			for (const TilemapTile& tile : data.tiles) {
				if (!tile.texture) {
					continue;
				}

				V2_float visual_size{
					tile.pixel_size.IsPositive()
						? V2_float{
							static_cast<float>(tile.pixel_size.x),
							static_cast<float>(tile.pixel_size.y),
						}
						: data.cell_size
				};

				V2_float anchor{ tilemap.CellToWorld(tile.coordinate) + tile.offset };
				V2_float center{ anchor + GetOffset(tile.origin, visual_size) };
				V2_float mn{ center - visual_size * 0.5f };
				V2_float mx{ center + visual_size * 0.5f };

				ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
				ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };

				// Large tilemaps may contain tens of thousands of cells. Do not
				// build ImGui draw commands or search asset records off screen.
				if (std::max(p0.x, p1.x) < viewport_min.x ||
					std::min(p0.x, p1.x) > viewport_max.x ||
					std::max(p0.y, p1.y) < viewport_min.y ||
					std::min(p0.y, p1.y) > viewport_max.y) {
					continue;
				}

				auto record_it{ std::ranges::find_if(records, [&](const auto& record) {
					return record.kind == AssetKind::Texture &&
						   record.key == static_cast<const AssetKey&>(tile.texture);
				}) };

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
			// streaming/debug settings. They use the same translucent red convention as the
			// sandbox paint demo.
			for (V2_int cell : data.exclusion_mask) {
				V2_float mn{ tilemap.CellToWorld(cell) };
				V2_float mx{ mn + data.cell_size };
				ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
				ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };
				if (std::max(p0.x, p1.x) < viewport_min.x ||
					std::min(p0.x, p1.x) > viewport_max.x ||
					std::max(p0.y, p1.y) < viewport_min.y ||
					std::min(p0.y, p1.y) > viewport_max.y) {
					continue;
				}

				draw->AddRectFilled(p0, p1, IM_COL32(255, 26, 26, 46));
				draw->AddRect(p0, p1, IM_COL32(255, 72, 72, 120), 0.0f, 0, 1.0f);
			}
		}
	}
}

// Highlight only the actual sprites produced by a custom streamed generator,
// not its potentially infinite underlying candidate lattice.
void DrawCustomGeneratorSelection(
	PaintGenerator generator, ImDrawList* draw, const FrameContext& frame,
	Viewport presentation_viewport, V2_float view_min, V2_float view_max
) {
	const auto& source{ generator.Get<WorldGeneratorSource>() };
	const auto& data{ generator.GetData() };
	if (!source.preview_texture || !source.preview_size.IsPositive() ||
		!data.grid_size.IsPositive() ||
		data.recipe.source_kind != PaintGeneratorSourceKind::Noise) {
		return;
	}

	auto hash_unit = [](V2_int cell, std::uint32_t salt) {
		std::uint32_t hash{ salt ^ (static_cast<std::uint32_t>(cell.x) * 0x85ebca6bu) ^
			(static_cast<std::uint32_t>(cell.y) * 0xc2b2ae35u) };
		hash ^= hash >> 16u;
		hash *= 0x7feb352du;
		hash ^= hash >> 15u;
		hash *= 0x846ca68bu;
		hash ^= hash >> 16u;
		return static_cast<float>(hash & 0x00ffffffu) / static_cast<float>(0x01000000u);
	};

	V2_float origin{ GetWorldPosition(generator) + data.grid_offset + source.preview_origin };
	V2_float spacing{ data.grid_size };
	V2_float margin{ spacing + source.preview_size };
	V2_int first{
		static_cast<int>(std::floor((view_min.x - margin.x - origin.x) / spacing.x)),
		static_cast<int>(std::floor((view_min.y - margin.y - origin.y) / spacing.y))
	};
	V2_int last{
		static_cast<int>(std::ceil((view_max.x + margin.x - origin.x) / spacing.x)),
		static_cast<int>(std::ceil((view_max.y + margin.y - origin.y) / spacing.y))
	};
	if (static_cast<std::int64_t>(last.x - first.x + 1) *
		static_cast<std::int64_t>(last.y - first.y + 1) > 4096) {
		return;
	}

	for (int y{ first.y }; y <= last.y; ++y) {
		for (int x{ first.x }; x <= last.x; ++x) {
			V2_int cell{ x, y };
			if (generator.IsSuppressed(cell)) {
				continue;
			}
			V2_float position{ origin + V2_float{ static_cast<float>(x) * spacing.x,
				static_cast<float>(y) * spacing.y } };
			position += V2_float{
				(hash_unit(cell, source.preview_jitter_salt) * 2.0f - 1.0f) * spacing.x * 0.46f,
				(hash_unit(cell, source.preview_jitter_salt ^ 0x5e19c713u) * 2.0f - 1.0f) * spacing.y * 0.46f
			};
			if (source.preview_exclusion_half_size.IsPositive() &&
				std::abs(position.x) < source.preview_exclusion_half_size.x &&
				std::abs(position.y) < source.preview_exclusion_half_size.y) {
				continue;
			}
			float noise{ SamplePaintGeneratorNoise(data.recipe.noise, position) };
			bool passes_noise{ false };
			for (const auto& region : data.recipe.noise.thresholds) {
				if (!region.enabled) {
					continue;
				}
				float low{ std::min(region.minimum, region.maximum) };
				float high{ std::max(region.minimum, region.maximum) };
				if (noise >= low && (noise < high || (high >= 1.0f && noise <= 1.0f))) {
					passes_noise = true;
					break;
				}
			}
			if (!passes_noise || hash_unit(cell, source.preview_density_salt) >=
				std::clamp(data.recipe.density, 0.0f, 1.0f)) {
				continue;
			}
			V2_float min{ position - source.preview_size * 0.5f };
			V2_float max{ position + source.preview_size * 0.5f };
			draw->AddRectFilled(
				ToImGui(WorldToScreen(min, frame, presentation_viewport)),
				ToImGui(WorldToScreen(max, frame, presentation_viewport)),
				IM_COL32(78, 190, 255, 36)
			);
			draw->AddRect(
				ToImGui(WorldToScreen(min, frame, presentation_viewport)),
				ToImGui(WorldToScreen(max, frame, presentation_viewport)),
				kSelection, 0.0f, 0, 2.0f
			);
		}
	}
}

void PaintEditor::DrawGenerators(
	EditorContext& ctx, Scene& scene, ImDrawList* draw, Viewport image_viewport,
	Viewport presentation_viewport, const FrameContext& frame
) const {
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::unordered_map<std::string, std::optional<PrefabGeneratorPreview>> prefab_preview_cache;
	Entity hierarchy_selected{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	Entity selected_generator{
		hierarchy_selected && IsPaintGenerator(hierarchy_selected)
			? hierarchy_selected
			: (!hierarchy_selected && selected_generator_.has_value()
				? scene.GetEntity(*selected_generator_)
				: Entity{})
	};
	V2_float view_a{ ScreenToWorld(image_viewport.position, frame, presentation_viewport) };
	V2_float view_b{
		ScreenToWorld(image_viewport.position + image_viewport.size, frame, presentation_viewport)
	};
	float view_min_x{ std::min(view_a.x, view_b.x) };
	float view_max_x{ std::max(view_a.x, view_b.x) };
	float view_min_y{ std::min(view_a.y, view_b.y) };
	float view_max_y{ std::max(view_a.y, view_b.y) };

	auto line_cells = [](V2_int a, V2_int b, int thickness, int spacing) {
		std::vector<V2_int> result;
		int dx{ std::abs(b.x - a.x) };
		int dy{ -std::abs(b.y - a.y) };
		int sx{ a.x < b.x ? 1 : -1 };
		int sy{ a.y < b.y ? 1 : -1 };
		int error{ dx + dy };
		int step{};
		for (;;) {
			if (step % std::max(1, spacing) == 0) {
				int half{ std::max(1, thickness) / 2 };
				for (int y{ -half }; y <= half; ++y) {
					for (int x{ -half }; x <= half; ++x) {
						result.emplace_back(a + V2_int{ x, y });
					}
				}
			}
			if (a == b) {
				break;
			}
			int e2{ 2 * error };
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

	{
		for (Entity entity : scene.Entities()) {
			if (!IsPaintGenerator(entity)) {
				continue;
			}
			PaintGenerator generator{ entity };
			const auto& data{ generator.GetData() };
			if (!data.enabled || !data.grid_size.IsPositive()) {
				continue;
			}
			// A custom world generator is drawn by the scene's editor preview.
			// Only its actual generated sprite bounds receive a selection overlay.
			if (entity.Has<WorldGeneratorSource>()) {
				if (entity == selected_generator) {
					DrawCustomGeneratorSelection(
						generator, draw, frame, presentation_viewport,
						{ view_min_x, view_min_y }, { view_max_x, view_max_y }
					);
				}
				continue;
			}

			// Always draw the generated tile/prefab preview in editor mode.
			// Selection affects only guides, not the generated content.

			Tilemap generator_target{ generator.GetTargetTilemap() };
			bool generates_tiles{ static_cast<bool>(generator_target) };
			V2_float entity_offset{ GetWorldPosition(generator) };
			V2_float origin{ data.grid_offset + entity_offset };
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
			V2_int visible_min{ to_cell({ view_min_x, view_min_y }) - V2_int{ 1, 1 } };
			V2_int visible_max{ to_cell({ view_max_x, view_max_y }) + V2_int{ 1, 1 } };
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
				V2_int a{ to_cell(data.start + entity_offset) };
				V2_int b{ to_cell(data.end + entity_offset) };
				int min_x{ std::min(a.x, b.x) }, max_x{ std::max(a.x, b.x) },
					min_y{ std::min(a.y, b.y) }, max_y{ std::max(a.y, b.y) };
				for (int y{ min_y }; y <= max_y; ++y) {
					for (int x{ min_x }; x <= max_x; ++x) {
						int l{ x - min_x }, r{ max_x - x }, t{ y - min_y }, bottom{ max_y - y };
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
								int thickness{ std::max(1, data.area_thickness) };
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
					V2_int center{ to_cell(point.position + entity_offset) };
					int diameter{ std::max(1, point.diameter) };
					int low{ -(diameter / 2) }, high{ low + diameter - 1 };
					float radius{ static_cast<float>(diameter) * 0.5f };
					for (int y{ low }; y <= high; ++y) {
						for (int x{ low }; x <= high; ++x) {
							if (data.brush_shape == PaintGeneratorBrushShape::Circle) {
								float offset{ BrushCircleCellOffset(diameter) };
								float dx{ static_cast<float>(x) + offset },
									dy{ static_cast<float>(y) + offset };
								if (std::sqrt(dx * dx + dy * dy) > radius) {
									continue;
								}
							}
							AddGeneratorBrushCell(
								data.recipe, center + V2_int{ x, y },
								[&](V2_int cell) { add_if_visible(cell); }
							);
						}
					}
				}
			}

			for (V2_int cell : cells) {
				if (generator.IsSuppressed(cell)) {
					continue;
				}
				if (generator_target && data.recipe.avoid_exclusion_mask &&
					generator_target.IsExcluded(cell)) {
					continue;
				}
				if (data.recipe.coverage == PaintGeneratorCoverageMode::RandomDensity &&
					Hash01(cell) > data.recipe.density) {
					continue;
				}
				V2_float mn{ cell_world(cell) };
				V2_float mx{ mn + data.grid_size };
				V2_float center{ mn + data.grid_size * 0.5f };
				float noise_value{ data.recipe.source_kind == PaintGeneratorSourceKind::Noise
									   ? SamplePaintGeneratorNoise(data.recipe.noise, center)
									   : 0.0f };
				if (entity == selected_generator && data.recipe.show_noise_preview &&
					data.recipe.source_kind == PaintGeneratorSourceKind::Noise) {
					int shade{ static_cast<int>(std::round(noise_value * 255.0f)) };
					int alpha{ static_cast<int>(
						std::round(std::clamp(data.recipe.noise_preview_alpha, 0.0f, 1.0f) * 255.0f)
					) };
					draw->AddRectFilled(
						ToImGui(WorldToScreen(mn, frame, presentation_viewport)),
						ToImGui(WorldToScreen(mx, frame, presentation_viewport)),
						IM_COL32(shade, shade, shade, alpha)
					);
				}
				std::optional<PaintGeneratorTileSource> tile;
				std::optional<PrefabKey> prefab;
				Origin entity_origin{ data.recipe.entity_origin };
				switch (data.recipe.source_kind) {
					case PaintGeneratorSourceKind::Single:
						tile   = data.recipe.tile;
						prefab = data.recipe.prefab;
						break;
					case PaintGeneratorSourceKind::WeightedSet: {
						if (generates_tiles) {
							auto i{ ChooseWeightedIndex(
								data.recipe.weighted_tiles, cell,
								[](const auto& e) { return e.weight; }
							) };
							if (i) {
								tile = data.recipe.weighted_tiles[*i].source;
							}
						} else {
							auto i{ ChooseWeightedIndex(
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
						int index{ AutotileIndex(
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
							float lo{ std::min(region.minimum, region.maximum) },
								hi{ std::max(region.minimum, region.maximum) };
							if (noise_value < lo ||
								(noise_value >= hi && !(hi >= 0.99999f && noise_value <= 1.0f))) {
								continue;
							}
							entity_origin = region.origin;
							if (region.source_kind == PaintGeneratorSourceKind::WeightedSet) {
								if (generates_tiles) {
									auto i{ ChooseWeightedIndex(
										region.weighted_tiles, cell,
										[](const auto& e) { return e.weight; }
									) };
									if (i) {
										tile = region.weighted_tiles[*i].source;
									}
								} else {
									auto i{ ChooseWeightedIndex(
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
				bool selected{ selected_generator && selected_generator == entity };
				if (generates_tiles && tile.has_value()) {
					V2_float visual_size{ tile->pixel_size.IsPositive()
											  ? V2_float{ tile->pixel_size }
											  : data.grid_size };
					if (data.recipe.tile_placement == PaintGeneratorTilePlacementMode::Tile) {
						V2_int footprint{
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
						V2_int lattice_origin{ to_cell(data.start + entity_offset) };
						if (FloorMod(cell.x - lattice_origin.x, footprint.x) != 0 ||
							FloorMod(cell.y - lattice_origin.y, footprint.y) != 0) {
							continue;
						}
					}
					auto record_it{ std::ranges::find_if(records, [&](const auto& r) {
						return r.kind == AssetKind::Texture &&
							   r.key == static_cast<const AssetKey&>(tile->texture);
					}) };
					V2_float tile_center{ mn + GetOffset(tile->origin, visual_size) };
					V2_float tile_min{ tile_center - visual_size * 0.5f };
					V2_float tile_max{ tile_center + visual_size * 0.5f };
					if (record_it != records.end() && record_it->preview.has_value()) {
						const auto& uv{ tile->texture_coordinates };
						ImVec2 p0{ ToImGui(WorldToScreen(tile_min, frame, presentation_viewport)) };
						ImVec2 p1{ ToImGui(WorldToScreen(tile_max, frame, presentation_viewport)) };
						draw->AddImageQuad(
							static_cast<ImTextureID>(record_it->preview->texture), p0,
							{ p1.x, p0.y }, p1, { p0.x, p1.y }, ToImGui(uv[0]), ToImGui(uv[1]),
							ToImGui(uv[2]), ToImGui(uv[3]), IM_COL32(255, 255, 255, 255)
						);
					}
					if (selected) {
						draw->AddRectFilled(
							ToImGui(WorldToScreen(tile_min, frame, presentation_viewport)),
							ToImGui(WorldToScreen(tile_max, frame, presentation_viewport)),
							IM_COL32(78, 190, 255, 32)
						);
						draw->AddRect(
							ToImGui(WorldToScreen(tile_min, frame, presentation_viewport)),
							ToImGui(WorldToScreen(tile_max, frame, presentation_viewport)),
							kSelection, 0.0f, 0, 2.0f
						);
					}
				} else if (!generates_tiles && prefab.has_value()) {
					float selector{ Hash01(cell) };
					std::optional<float> preview_rotation{
						data.recipe.random_rotation
							? std::optional<float>{ data.recipe.rotation_min +
													(data.recipe.rotation_max -
													 data.recipe.rotation_min) *
														selector }
							: std::nullopt
					};
					std::optional<float> preview_scale{
						data.recipe.random_scale ? std::optional<float>{ data.recipe.scale_min +
																		 (data.recipe.scale_max -
																		  data.recipe.scale_min) *
																			 selector }
												 : std::nullopt
					};
					V2_float placement{ mn + GetOffset(entity_origin, data.grid_size) };
					bool drawn{ DrawPrefabGeneratorPreview(
						ctx, prefab_preview_cache, draw, *prefab, placement, preview_rotation,
						preview_scale, frame, presentation_viewport
					) };
					if (!drawn) {
						// A prefab without a drawable texture still gets a useful fallback.
						ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
						ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };
						draw->AddRectFilled(p0, p1, IM_COL32(96, 214, 122, 42));
						draw->AddRect(p0, p1, IM_COL32(104, 234, 132, 210), 0.0f, 0, 1.2f);
						std::string name{ PrefabDisplayName(*prefab) };
						if (!name.empty() && p1.x - p0.x > 24.0f && p1.y - p0.y > 16.0f) {
							draw->AddText(
								{ p0.x + 3.0f, p0.y + 2.0f }, IM_COL32(218, 255, 225, 220),
								name.c_str()
							);
						}
					}
					if (selected) {
						V2_float size{ data.grid_size };
						auto preview_it{ prefab_preview_cache.find(prefab->value) };
						if (preview_it != prefab_preview_cache.end() &&
							preview_it->second.has_value()) {
							size = V2_float{ preview_it->second->texture_size };
						}
						if (preview_scale.has_value()) {
							size *= *preview_scale;
						}
						V2_float bounds_min{ placement - size * 0.5f };
						V2_float bounds_max{ placement + size * 0.5f };
						draw->AddRectFilled(
							ToImGui(WorldToScreen(bounds_min, frame, presentation_viewport)),
							ToImGui(WorldToScreen(bounds_max, frame, presentation_viewport)),
							IM_COL32(78, 190, 255, 32)
						);
						draw->AddRect(
							ToImGui(WorldToScreen(bounds_min, frame, presentation_viewport)),
							ToImGui(WorldToScreen(bounds_max, frame, presentation_viewport)),
							kSelection, 0.0f, 0, 2.0f
						);
					}
				}
				// Selection outlines are drawn around emitted visuals in the
				// tile/prefab branches, never around empty candidate cells.
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

	bool tile_mode{ IsTileMode(scene) };

	std::unordered_set<V2_int> cells;
	V2_int first{ WorldToActiveCell(scene, stroke_.start_world) };
	V2_int last{ WorldToActiveCell(scene, stroke_.current_world) };
	if (tool_ == PaintTool::Brush) {
		auto add_brush_cell = [&](V2_int center) {
			for (V2_int cell : BrushCells(center)) {
				cells.insert(cell);
			}
		};
		for (V2_float point : stroke_.generator_points) {
			add_brush_cell(WorldToActiveCell(scene, point));
		}

		std::optional<V2_int> anchor{ stroke_.has_last_cell
										  ? std::optional<V2_int>{ stroke_.last_cell }
										  : std::nullopt };
		V2_int current_cell{ ResolveStableBrushCenter(
			WorldToActiveCell(scene, stroke_.current_world), brush_diameter_, anchor
		) };
		if (!stroke_.generator_points.empty()) {
			V2_int previous_cell{ WorldToActiveCell(scene, stroke_.generator_points.back()) };
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

	V2_int minimum{ std::min(first.x, last.x), std::min(first.y, last.y) };
	V2_int maximum{ std::max(first.x, last.x), std::max(first.y, last.y) };
	V2_float grid{ ActiveGridSize(scene) };
	Tilemap target{ tile_mode ? ResolveTargetTilemap(scene) : Tilemap{} };
	auto records{ ::ptgn::impl::AssetAccessor{ ctx.editor.GetAssetManager() }.GetAssets() };
	std::unordered_map<std::string, std::optional<PrefabGeneratorPreview>> prefab_preview_cache;

	for (V2_int cell : cells) {
		if (!CoveragePass(cell, minimum, maximum)) {
			continue;
		}
		if (target && recipe_.avoid_exclusion_mask && target.IsExcluded(cell)) {
			continue;
		}

		V2_float cell_min{ ActiveCellToWorld(scene, cell) };
		V2_float cell_max{ cell_min + grid };
		V2_float cell_center{ cell_min + grid * 0.5f };
		draw->AddRect(
			ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)),
			ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)), kPreview, 0.0f, 0, 2.0f
		);

		if (recipe_.show_noise_preview && recipe_.source_kind == PaintSourceKind::Noise) {
			float value{ NoiseValue(cell_center, recipe_.noise) };
			int shade{ static_cast<int>(std::round(value * 255.0f)) };
			int alpha{ static_cast<int>(
				std::round(std::clamp(recipe_.noise_preview_alpha, 0.0f, 1.0f) * 255.0f)
			) };
			draw->AddRectFilled(
				ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)),
				ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)),
				IM_COL32(shade, shade, shade, alpha)
			);
		}
		std::optional<PaintTileSource> tile;
		std::optional<PrefabKey> prefab;
		Origin origin{ tile_mode ? recipe_.tile_origin : recipe_.entity_origin };
		switch (recipe_.source_kind) {
			case PaintSourceKind::Single:
				if (tile_mode && tile_source_) {
					tile = tile_source_;
				} else if (!tile_mode && prefab_source_) {
					prefab = prefab_source_;
				}
				break;
			case PaintSourceKind::WeightedSet:
				if (tile_mode) {
					if (const auto* set{ FindWeightedTileSet(recipe_.weighted_tile_set_name) }) {
						auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
							return e.weight;
						}) };
						if (i) {
							tile = set->entries[*i].source;
						}
					}
				} else {
					if (const auto* set{
							FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) }) {
						auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
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
				if (const auto* rules{
						FindAutotileRuleSet(recipe_.autotile_texture, recipe_.autotile_format) }) {
					int index{ AutotileIndex(
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
						if (tile_mode) {
							if (const auto* set{
									FindWeightedTileSet(region->weighted_tile_set_name) }) {
								auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
									return e.weight;
								}) };
								if (i) {
									tile = set->entries[*i].source;
								}
							}
						} else {
							if (const auto* set{
									FindWeightedPrefabSet(region->weighted_prefab_set_name) }) {
								auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
									return e.weight;
								}) };
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

		if (tile_mode && tile.has_value()) {
			V2_float visual_size{ tile->pixel_size.IsPositive() ? V2_float{ tile->pixel_size }
																: grid };
			if (recipe_.tile_placement == PaintTilePlacementMode::Tile) {
				V2_int footprint{
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
			auto record{ std::ranges::find_if(records, [&](const auto& candidate) {
				return candidate.kind == AssetKind::Texture &&
					   candidate.key == static_cast<const AssetKey&>(tile->texture);
			}) };
			V2_float center{ cell_min + GetOffset(origin, visual_size) };
			V2_float minimum_world{ center - visual_size * 0.5f };
			V2_float maximum_world{ center + visual_size * 0.5f };
			ImVec2 p0{ ToImGui(WorldToScreen(minimum_world, frame, presentation_viewport)) };
			ImVec2 p1{ ToImGui(WorldToScreen(maximum_world, frame, presentation_viewport)) };
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
		} else if (!tile_mode && prefab.has_value()) {
			float selector{ Hash01(cell) };
			std::optional<float> preview_rotation{
				recipe_.random_rotation
					? std::optional<float>{ recipe_.rotation_min +
											(recipe_.rotation_max - recipe_.rotation_min) *
												selector }
					: std::nullopt
			};
			std::optional<float> preview_scale{
				recipe_.random_scale
					? std::optional<float>{ recipe_.scale_min +
											(recipe_.scale_max - recipe_.scale_min) * selector }
					: std::nullopt
			};
			V2_float placement{ cell_min + GetOffset(origin, grid) };
			if (!DrawPrefabGeneratorPreview(
					ctx, prefab_preview_cache, draw, *prefab, placement, preview_rotation,
					preview_scale, frame, presentation_viewport
				)) {
				ImVec2 p0{ ToImGui(WorldToScreen(cell_min, frame, presentation_viewport)) };
				ImVec2 p1{ ToImGui(WorldToScreen(cell_max, frame, presentation_viewport)) };
				draw->AddRectFilled(p0, p1, IM_COL32(96, 214, 122, 42));
				draw->AddRect(p0, p1, IM_COL32(104, 234, 132, 210), 0.0f, 0, 1.2f);
				std::string name{ PrefabDisplayName(*prefab) };
				if (!name.empty() && p1.x - p0.x > 24.0f && p1.y - p0.y > 16.0f) {
					draw->AddText(
						{ p0.x + 3.0f, p0.y + 2.0f }, IM_COL32(218, 255, 225, 220), name.c_str()
					);
				}
			}
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
	// Infinite generators are selected by highlighting their generated artwork.
	// Do not overlay the infinite authoring lattice on top of that artwork.
	if (selected_generator_.has_value()) {
		Entity selected{ scene.GetEntity(*selected_generator_) };
		if (selected && IsPaintGenerator(selected) &&
			PaintGenerator{ selected }.GetData().geometry == PaintGeneratorGeometry::Infinite) {
			return;
		}
	}
	V2_float size{ ActiveGridSize(scene) };
	if (!size.IsPositive()) {
		return;
	}
	V2_float origin{ ActiveGridOrigin(scene) };
	V2_float wa{ ScreenToWorld(image_viewport.position, frame, presentation_viewport) };
	V2_float wb{
		ScreenToWorld(image_viewport.position + image_viewport.size, frame, presentation_viewport)
	};
	float min_x{ std::min(wa.x, wb.x) };
	float max_x{ std::max(wa.x, wb.x) };
	float min_y{ std::min(wa.y, wb.y) };
	float max_y{ std::max(wa.y, wb.y) };
	int first_x{ static_cast<int>(std::floor((min_x - origin.x) / size.x)) - 1 };
	int last_x{ static_cast<int>(std::ceil((max_x - origin.x) / size.x)) + 1 };
	int first_y{ static_cast<int>(std::floor((min_y - origin.y) / size.y)) - 1 };
	int last_y{ static_cast<int>(std::ceil((max_y - origin.y) / size.y)) + 1 };
	if (last_x - first_x > 2048 || last_y - first_y > 2048) {
		return;
	}

	ImU32 minor{ ImGui::GetColorU32(
		ImVec4{ grid_minor_color_[0], grid_minor_color_[1], grid_minor_color_[2],
				grid_minor_color_[3] }
	) };
	ImU32 major_color{ ImGui::GetColorU32(
		ImVec4{ grid_major_color_[0], grid_major_color_[1], grid_major_color_[2],
				grid_major_color_[3] }
	) };
	for (int x{ first_x }; x <= last_x; ++x) {
		float world_x{ origin.x + static_cast<float>(x) * size.x };
		bool major{ x % std::max(1, grid_major_every_) == 0 };
		draw->AddLine(
			ToImGui(WorldToScreen({ world_x, min_y }, frame, presentation_viewport)),
			ToImGui(WorldToScreen({ world_x, max_y }, frame, presentation_viewport)),
			major ? major_color : minor, major ? grid_major_thickness_ : grid_minor_thickness_
		);
	}
	for (int y{ first_y }; y <= last_y; ++y) {
		float world_y{ origin.y + static_cast<float>(y) * size.y };
		bool major{ y % std::max(1, grid_major_every_) == 0 };
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
		auto bounds{ EntitySelectionBounds(entity) };
		if (!bounds.has_value()) {
			continue;
		}
		ImVec2 p0{ ToImGui(WorldToScreen(bounds->min, frame, presentation_viewport)) };
		ImVec2 p1{ ToImGui(WorldToScreen(bounds->max, frame, presentation_viewport)) };
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
				PaintSelectionRect bounds{ TileSelectionBounds(map, *tile) };
				ImVec2 p0{ ToImGui(WorldToScreen(bounds.min, frame, presentation_viewport)) };
				ImVec2 p1{ ToImGui(WorldToScreen(bounds.max, frame, presentation_viewport)) };
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

	V2_float start_screen{ WorldToScreen(selection_drag_start_, frame, presentation_viewport) };
	V2_float current_screen{ WorldToScreen(current, frame, presentation_viewport) };
	V2_float screen_delta{ current_screen - start_screen };
	if (std::sqrt(screen_delta.x * screen_delta.x + screen_delta.y * screen_delta.y) < 4.0f) {
		return;
	}

	PaintSelectionRect bounds{ MakeSelectionRect(selection_drag_start_, current) };
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
	bool mask_operation{ recipe_.operation == PaintBrushOperation::ExclusionMask &&
						 (tool_ == PaintTool::Brush || tool_ == PaintTool::Line ||
						  tool_ == PaintTool::Rectangle || tool_ == PaintTool::Fill ||
						  tool_ == PaintTool::Erase) };
	ImU32 color{ mask_operation ? IM_COL32(255, 72, 72, 220) : kPreview };

	V2_float size{ ActiveGridSize(scene) };
	V2_int hovered_cell{ WorldToActiveCell(scene, mouse_world) };
	V2_int cell{ hovered_cell };
	if (tool_ == PaintTool::Select && select_mode_ == PaintSelectMode::Brush) {
		if (stroke_.has_last_cell) {
			cell = stroke_.last_cell;
		}
	} else if (tool_ == PaintTool::Brush || tool_ == PaintTool::Erase) {
		std::optional<V2_int> anchor{ stroke_.has_last_cell
										  ? std::optional<V2_int>{ stroke_.last_cell }
										  : std::nullopt };
		cell = ResolveStableBrushCenter(hovered_cell, brush_diameter_, anchor);
	}

	auto draw_cell = [&](V2_int c) {
		V2_float mn{ ActiveCellToWorld(scene, c) };
		V2_float mx{ mn + size };

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
		auto brush_cells{ BrushCells(cell) };
		if (recipe_.source_kind == PaintSourceKind::Autotile &&
			recipe_.autotile_format == PaintAutotileFormat::Classic15 &&
			recipe_.operation != PaintBrushOperation::ExclusionMask) {
			// Rendered tiles occupy the four cells above/left of each logical
			// occupancy vertex. Show the complete visual footprint, not just the
			// vertices that are written to the tilemap.
			std::unordered_set<V2_int> preview;
			preview.reserve(brush_cells.size() * 4);
			for (V2_int logical : brush_cells) {
				preview.insert(logical);
				preview.insert(logical + V2_int{ -1, 0 });
				preview.insert(logical + V2_int{ 0, -1 });
				preview.insert(logical + V2_int{ -1, -1 });
			}
			for (V2_int c : preview) {
				draw_cell(c);
			}
		} else {
			for (V2_int c : brush_cells) {
				draw_cell(c);
			}
		}
	} else if ((tool_ == PaintTool::Line || tool_ == PaintTool::Rectangle) && stroke_.active) {
		V2_int start{ WorldToActiveCell(scene, stroke_.start_world) };
		auto cells{ tool_ == PaintTool::Line ? LineCells(start, cell)
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

void PaintEditor::BeginStroke(EditorContext& ctx, Scene& scene, V2_float world) {
	V2_int hovered_cell{ WorldToActiveCell(scene, world) };
	std::optional<V2_int> anchor{ stroke_.has_last_cell ? std::optional<V2_int>{ stroke_.last_cell }
														: std::nullopt };
	V2_int start_cell{ (tool_ == PaintTool::Brush || tool_ == PaintTool::Erase)
						   ? ResolveStableBrushCenter(hovered_cell, brush_diameter_, anchor)
						   : hovered_cell };
	V2_float anchored_world{ ActiveCellToWorld(scene, start_cell) + ActiveGridSize(scene) * 0.5f };

	stroke_		   = {};
	stroke_.active = true;
	if (!IsTileMode(scene) && recipe_.operation == PaintBrushOperation::ExclusionMask) {
		stroke_.entity_mask_before =
			GetEntityExclusionCells(ctx.project_state.paint, scene.GetTag());
		stroke_.entity_mask_capture = true;
		stroke_.entity_mask_cells.insert(
			stroke_.entity_mask_before.begin(), stroke_.entity_mask_before.end()
		);
	}
	stroke_.start_world	  = tool_ == PaintTool::Brush ? anchored_world : world;
	stroke_.current_world = tool_ == PaintTool::Brush ? anchored_world : world;
	stroke_.last_cell	  = start_cell;
	stroke_.has_last_cell = true;
	if (tool_ == PaintTool::Brush && recipe_.commit_mode == PaintCommitMode::KeepGenerator) {
		stroke_.generator_points.push_back(anchored_world);
	}

	// A tile stroke is valid only when a selected Tilemap already exists.
	if (Tilemap map{ ResolveTargetTilemap(scene) }) {
		stroke_.tilemap		   = map.Get<UUID>();
		stroke_.tilemap_before = map.GetData();
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
	if (tilemap.IsExcluded(cell)) {
		return;
	}
	if (recipe_.coverage == PaintCoverageMode::RandomDensity && Hash01(cell) > recipe_.density) {
		return;
	}

	if (recipe_.source_kind == PaintSourceKind::Autotile) {
		const PaintAutotileRuleSet* rules{ ResolveAutotileRuleSet(ctx) };
		if (!rules || rules->id == 0 || rules->tiles.empty()) {
			return;
		}
		std::optional<std::uint64_t> old_ruleset{ tilemap.GetTerrainRuleset(cell) };
		tilemap.SetTerrainRuleset(cell, rules->id);
		if (old_ruleset.has_value() && *old_ruleset != rules->id) {
			if (const PaintAutotileRuleSet* old{ FindAutotileRuleSet(*old_ruleset) }) {
				RecomputeAutotileAround(tilemap, cell, *old);
			}
		}
		RecomputeAutotileAround(tilemap, cell, *rules);
		return;
	}

	// Painting an ordinary tile replaces logical terrain at that cell. Recompute its old
	// neighborhood first so adjacent derived terrain variants remain correct.
	if (auto old_ruleset{ tilemap.GetTerrainRuleset(cell) }) {
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
				auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
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
			V2_float sample_world{ tilemap.CellToWorld(cell) + tilemap.GetData().cell_size * 0.5f };
			const PaintNoiseThreshold* region{ ResolveNoiseThreshold(sample_world) };
			if (!region) {
				return;
			}
			origin = region->origin;
			if (region->source_kind == PaintSourceKind::WeightedSet) {
				if (const auto* set{ FindWeightedTileSet(region->weighted_tile_set_name) }) {
					auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
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
		V2_float cell_size{ tilemap.GetData().cell_size };
		V2_int footprint{
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
		V2_int lattice_origin{ stroke_.active ? tilemap.WorldToCell(stroke_.start_world) : cell };
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
}

void PaintEditor::ApplyEntityAt(EditorContext& ctx, Scene& scene, V2_float world) {
	bool tile_mode{ IsTileMode(scene) };
	if (tile_mode) {
		return;
	}

	V2_int cell{ WorldToActiveCell(scene, world) };
	if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
		stroke_.entity_mask_cells.insert(cell);
		return;
	}
	if (IsEntityCellExcluded(ctx.project_state.paint, scene.GetTag(), cell)) {
		return;
	}
	if (recipe_.coverage == PaintCoverageMode::RandomDensity && Hash01(cell) > recipe_.density) {
		return;
	}

	PrefabKey source{};
	Origin origin{ recipe_.entity_origin };
	switch (recipe_.source_kind) {
		case PaintSourceKind::Single:	   source = prefab_source_; break;
		case PaintSourceKind::WeightedSet: {
			if (const auto* set{ FindWeightedPrefabSet(recipe_.weighted_prefab_set_name) }) {
				auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
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
			V2_float sample_world{ ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f };
			const PaintNoiseThreshold* region{ ResolveNoiseThreshold(sample_world) };
			if (!region) {
				return;
			}
			origin = region->origin;
			if (region->source_kind == PaintSourceKind::WeightedSet) {
				if (const auto* set{ FindWeightedPrefabSet(region->weighted_prefab_set_name) }) {
					auto i{ ChooseWeightedIndex(set->entries, cell, [](const auto& e) {
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

	// Scene::CreatePrefab() and AssetManager use the same residency path. Paint code does not
	// need to know how prefab assets are catalogued or loaded.
	auto& assets{ ctx.editor.GetAssetManager() };
	if (!assets.EnsurePrefabResident(source)) {
		return;
	}

	std::vector<Entity> occupied;
	for (Entity root : SceneRootEntities(scene)) {
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
			UUID uuid{ entity.Get<UUID>() };
			auto created_it{ std::ranges::find(stroke_.entities.created_roots, uuid) };
			if (created_it != stroke_.entities.created_roots.end()) {
				stroke_.entities.created_roots.erase(created_it);
			} else {
				stroke_.entities.deleted_roots.push_back(
					DeletedEntity{ .entity = SerializeEntity(entity) }
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
		for (Entity root : SceneRootEntities(scene)) {
			if (!root.Has<Transform>() || IsProtectedSceneEntity(scene, root) ||
				IsPaintGenerator(root)) {
				continue;
			}
			V2_float delta{ GetWorldPosition(root) - world };
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
	Transform world_transform{ GetWorldTransform(entity) };
	world_transform.position = world;
	SetWorldTransform(entity, world_transform);
	float selector{ Hash01(cell) };
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
	bool tile_mode{ IsTileMode(scene) };
	if (tile_mode) {
		if (Tilemap tilemap{ ResolveTargetTilemap(scene) }) {
			V2_int cell{ tilemap.WorldToCell(world) };
			if (stroke_.touched_cells.insert(cell).second) {
				ApplyTileAt(ctx, scene, tilemap, cell);
			}
		}
	} else {
		V2_int cell{ WorldToActiveCell(scene, world) };
		if (stroke_.touched_cells.insert(cell).second) {
			ApplyEntityAt(ctx, scene, world);
		}
	}
}

void PaintEditor::EraseAt(EditorContext&, Scene& scene, V2_float world) {
	bool tile_mode{ IsTileMode(scene) };
	std::optional<V2_int> anchor{ stroke_.has_last_cell ? std::optional<V2_int>{ stroke_.last_cell }
														: std::nullopt };
	V2_int center{
		ResolveStableBrushCenter(WorldToActiveCell(scene, world), brush_diameter_, anchor)
	};
	stroke_.last_cell	  = center;
	stroke_.has_last_cell = true;
	auto cells{ BrushCells(center) };

	std::unordered_set<V2_int> newly_touched;
	for (V2_int cell : cells) {
		if (stroke_.touched_cells.insert(cell).second) {
			newly_touched.insert(cell);
		}
	}

	if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
		if (!tile_mode) {
			for (V2_int cell : newly_touched) {
				stroke_.entity_mask_cells.erase(cell);
			}
		} else if (Tilemap map{ ResolveTargetTilemap(scene) }) {
			for (V2_int cell : newly_touched) {
				map.SetExcluded(cell, false);
			}
		}
		return;
	}

	auto capture_generator_before = [&](Entity entity) {
		UUID uuid{ entity.Get<UUID>() };
		if (std::ranges::none_of(stroke_.generator_before, [&](const auto& entry) {
				return entry.first == uuid;
			})) {
			stroke_.generator_before.emplace_back(
				uuid, entity.Get<::ptgn::impl::PaintGeneratorData>()
			);
		}
	};

	Tilemap active_tilemap{ tile_mode ? ResolveTargetTilemap(scene) : Tilemap{} };

	for (Entity entity : scene.Entities()) {
		if (!IsPaintGenerator(entity)) {
			continue;
		}

		PaintGenerator generator{ entity };
		if (!generator.GetData().enabled) {
			continue;
		}
		if (tile_mode && (!active_tilemap || generator.GetTargetTilemap() != active_tilemap)) {
			continue;
		}

		for (V2_int brush_cell : newly_touched) {
			V2_float sample{ ActiveCellToWorld(scene, brush_cell) + ActiveGridSize(scene) * 0.5f };
			if (!GeneratorContainsWorld(entity, sample)) {
				continue;
			}
			capture_generator_before(entity);
			generator.SetSuppressed(GeneratorCellAtWorld(entity, sample), true);
		}
	}

	if (tile_mode) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		for (V2_int cell : newly_touched) {
			if (auto terrain{ map.GetTerrainRuleset(cell) }) {
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
	for (Entity root : SceneRootEntities(scene)) {
		if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
			!root.Has<Transform>()) {
			continue;
		}
		V2_int entity_cell{ WorldToActiveCell(scene, GetWorldPosition(root)) };
		if (std::ranges::contains(newly_touched, entity_cell)) {
			erase.emplace_back(root);
		}
	}
	for (Entity entity : erase) {
		UUID uuid{ entity.Get<UUID>() };
		auto created_it{ std::ranges::find(stroke_.entities.created_roots, uuid) };
		if (created_it != stroke_.entities.created_roots.end()) {
			stroke_.entities.created_roots.erase(created_it);
		} else {
			stroke_.entities.deleted_roots.push_back(
				DeletedEntity{
					.entity = SerializeEntity(entity),
				}
			);
		}
		entity.Destroy();
	}
	if (!erase.empty()) {
		scene.Refresh();
	}
}

void PaintEditor::FillAt(
	EditorContext& ctx, Scene& scene, V2_float world, Viewport image_viewport,
	Viewport presentation_viewport, const FrameContext& frame
) {
	bool tile_mode{ IsTileMode(scene) };

	auto viewport_corners{ ViewportWorldCorners(image_viewport, presentation_viewport, frame) };
	V2_float minimum{ viewport_corners.front() };
	V2_float maximum{ minimum };
	for (V2_float vertex : viewport_corners) {
		minimum = Min(minimum, vertex);
		maximum = Max(maximum, vertex);
	}

	auto fill_visible_cells = [&](auto to_cell, auto cell_to_world, V2_float cell_size,
								  auto apply) {
		V2_int first{ to_cell(minimum) - V2_int{ 1, 1 } };
		V2_int last{ to_cell(maximum) + V2_int{ 1, 1 } };
		std::size_t filled{};
		for (int y{ first.y }; y <= last.y && filled < kMaxFloodCells; ++y) {
			for (int x{ first.x }; x <= last.x && filled < kMaxFloodCells; ++x) {
				V2_int cell{ x, y };
				if (!CellVisibleInViewport(
						cell_to_world(cell), cell_size, image_viewport, presentation_viewport, frame
					)) {
					continue;
				}
				apply(cell);
				++filled;
			}
		}
	};

	if (tile_mode) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
			fill_visible_cells(
				[&](V2_float point) { return map.WorldToCell(point); },
				[&](V2_int cell) { return map.CellToWorld(cell); }, map.GetData().cell_size,
				[&](V2_int cell) { map.SetExcluded(cell, true); }
			);
			return;
		}
		// Flood matching uses the logical terrain identity. Display UVs vary at each
		// autotile edge, so matching rendered tiles would stop at the first corner.
		const auto& data{ map.GetData() };
		std::unordered_map<V2_int, const TilemapTile*> original_tiles;
		original_tiles.reserve(data.tiles.size() * 2 + 1);
		for (const TilemapTile& tile : data.tiles) {
			original_tiles.insert_or_assign(tile.coordinate, &tile);
		}
		std::unordered_map<V2_int, std::uint64_t> original_terrain;
		original_terrain.reserve(data.terrain.size() * 2 + 1);
		for (const TilemapTerrainCell& cell : data.terrain) {
			original_terrain.insert_or_assign(cell.coordinate, cell.ruleset_id);
		}

		V2_int start{ map.WorldToCell(world) };
		auto start_terrain{ original_terrain.find(start) };
		auto start_display{ original_tiles.find(start) };
		const TilemapTile* start_tile{ start_display == original_tiles.end()
										   ? nullptr
										   : start_display->second };
		// A derived dual grid display tile may cover a logically empty cell.
		if (start_terrain == original_terrain.end() && start_tile &&
			start_tile->terrain_ruleset_id) {
			start_tile = nullptr;
		}
		if (recipe_.operation == PaintBrushOperation::Erase &&
			start_terrain == original_terrain.end() && !start_tile) {
			return;
		}

		V2_int first{ map.WorldToCell(minimum) - V2_int{ 1, 1 } };
		V2_int last{ map.WorldToCell(maximum) + V2_int{ 1, 1 } };
		bool empty_start{ start_terrain == original_terrain.end() && !start_tile };

		auto matches = [&](V2_int cell) {
			if (auto it{ original_terrain.find(cell) }; it != original_terrain.end()) {
				return start_terrain != original_terrain.end() &&
					   it->second == start_terrain->second;
			}
			if (start_terrain != original_terrain.end()) {
				return false;
			}
			auto it{ original_tiles.find(cell) };
			const TilemapTile* current{ it == original_tiles.end() ? nullptr : it->second };
			if (current && current->terrain_ruleset_id) {
				current = nullptr;
			}
			if (!start_tile) {
				return !current;
			}
			return current && current->texture == start_tile->texture &&
				   current->texture_coordinates == start_tile->texture_coordinates;
		};

		std::vector<V2_int> queue{ start };
		std::unordered_set<V2_int> visited;
		std::vector<V2_int> region;
		visited.reserve(2048);
		region.reserve(2048);
		for (std::size_t head{}; head < queue.size(); ++head) {
			V2_int cell{ queue[head] };
			if (empty_start &&
				(cell.x < first.x || cell.x > last.x || cell.y < first.y || cell.y > last.y)) {
				continue;
			}
			if (!visited.insert(cell).second || !matches(cell)) {
				continue;
			}
			// Never apply a partial flood if the region is too large. This also
			// prevents accidentally filling unbounded empty world space.
			if (region.size() >= kMaxFloodCells) {
				return;
			}
			region.push_back(cell);
			queue.push_back(cell + V2_int{ 1, 0 });
			queue.push_back(cell + V2_int{ -1, 0 });
			queue.push_back(cell + V2_int{ 0, 1 });
			queue.push_back(cell + V2_int{ 0, -1 });
		}
		if (region.empty()) {
			return;
		}

		if (recipe_.operation == PaintBrushOperation::Erase ||
			recipe_.source_kind == PaintSourceKind::Autotile) {
			std::optional<std::uint64_t> new_ruleset;
			if (recipe_.operation != PaintBrushOperation::Erase) {
				const PaintAutotileRuleSet* rules{ ResolveAutotileRuleSet(ctx) };
				if (!rules || rules->tiles.empty()) {
					return;
				}
				new_ruleset = rules->id;
			}

			std::unordered_set<V2_int> excluded;
			if (new_ruleset) {
				excluded.reserve(map.GetData().exclusion_mask.size());
				excluded.insert(
					map.GetData().exclusion_mask.begin(), map.GetData().exclusion_mask.end()
				);
			}
			std::unordered_set<V2_int> changed_set;
			changed_set.reserve(region.size() * 2);
			std::vector<V2_int> changed;
			changed.reserve(region.size());
			for (V2_int cell : region) {
				if (new_ruleset && (excluded.contains(cell) ||
									(recipe_.coverage == PaintCoverageMode::RandomDensity &&
									 Hash01(cell) > recipe_.density))) {
					continue;
				}
				if (changed_set.insert(cell).second) {
					changed.push_back(cell);
				}
			}
			if (changed.empty()) {
				return;
			}

			// Edit the logical terrain vector in one pass. Calling
			// SetTerrainRuleset() separately per cell is quadratic on large fills.
			auto& terrain{ map.GetData().terrain };
			std::erase_if(terrain, [&](const TilemapTerrainCell& cell) {
				return changed_set.contains(cell.coordinate);
			});
			if (new_ruleset) {
				terrain.reserve(terrain.size() + changed.size());
				for (V2_int cell : changed) {
					terrain.push_back(
						TilemapTerrainCell{ .coordinate = cell, .ruleset_id = *new_ruleset }
					);
				}
			}
			// Rebuild the final rendered region exactly once. Solid interior tiles
			// have a known mask; only the outer edges need neighbor sampling.
			RecomputeAutotileRegion(map, changed);
			return;
		}

		for (V2_int cell : region) {
			ApplyTileAt(ctx, scene, map, cell);
		}
		return;
	}

	// The viewport bounds follow the editor presentation camera and its current zoom.
	if (recipe_.operation == PaintBrushOperation::ExclusionMask) {
		fill_visible_cells(
			[&](V2_float point) { return WorldToActiveCell(scene, point); },
			[&](V2_int cell) { return ActiveCellToWorld(scene, cell); }, ActiveGridSize(scene),
			[&](V2_int cell) { stroke_.entity_mask_cells.insert(cell); }
		);
		return;
	}
	V2_int min_cell{ WorldToActiveCell(scene, minimum) - V2_int{ 1, 1 } };
	V2_int max_cell{ WorldToActiveCell(scene, maximum) + V2_int{ 1, 1 } };
	V2_int start{ WorldToActiveCell(scene, world) };

	std::unordered_set<V2_int> occupied;
	for (Entity root : SceneRootEntities(scene)) {
		if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
			IsTilemap(root) || !root.Has<Transform>()) {
			continue;
		}
		occupied.insert(WorldToActiveCell(scene, GetWorldPosition(root)));
	}
	bool start_occupied{ occupied.contains(start) };
	if (start_occupied && recipe_.operation == PaintBrushOperation::Paint) {
		return;
	}

	if (recipe_.operation == PaintBrushOperation::Erase) {
		if (!start_occupied) {
			return;
		}

		std::vector<V2_int> queue{ start };
		std::unordered_set<V2_int> visited;
		std::unordered_set<V2_int> erase_cells;
		visited.reserve(1024);
		erase_cells.reserve(256);

		for (std::size_t head{}; head < queue.size() && visited.size() < kMaxFloodCells; ++head) {
			V2_int cell{ queue[head] };
			if (cell.x < min_cell.x || cell.x > max_cell.x || cell.y < min_cell.y ||
				cell.y > max_cell.y) {
				continue;
			}
			if (!visited.insert(cell).second || !occupied.contains(cell)) {
				continue;
			}

			erase_cells.insert(cell);
			queue.emplace_back(cell + V2_int{ 1, 0 });
			queue.emplace_back(cell + V2_int{ -1, 0 });
			queue.emplace_back(cell + V2_int{ 0, 1 });
			queue.emplace_back(cell + V2_int{ 0, -1 });
		}

		std::vector<Entity> erase;
		for (Entity root : SceneRootEntities(scene)) {
			if (!root || IsProtectedSceneEntity(scene, root) || IsPaintGenerator(root) ||
				IsTilemap(root) || !root.Has<Transform>()) {
				continue;
			}
			if (erase_cells.contains(WorldToActiveCell(scene, GetWorldPosition(root)))) {
				erase.emplace_back(root);
			}
		}

		for (Entity entity : erase) {
			UUID uuid{ entity.Get<UUID>() };
			auto created_it{ std::ranges::find(stroke_.entities.created_roots, uuid) };
			if (created_it != stroke_.entities.created_roots.end()) {
				stroke_.entities.created_roots.erase(created_it);
			} else {
				stroke_.entities.deleted_roots.push_back(
					DeletedEntity{
						.entity = SerializeEntity(entity),
					}
				);
			}
			entity.Destroy();
		}

		if (!erase.empty()) {
			scene.Refresh();
		}
		return;
	}

	std::vector<V2_int> queue{ start };
	std::unordered_set<V2_int> visited;
	visited.reserve(1024);
	for (std::size_t head{}; head < queue.size() && visited.size() < kMaxFloodCells; ++head) {
		V2_int cell{ queue[head] };
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
	bool tile_mode{ IsTileMode(scene) };

	if (tile_mode) {
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

	V2_int hovered_cell{ WorldToActiveCell(scene, world) };

	auto roots{ SceneRootEntities(scene) };

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

	V2_int hovered_cell{ WorldToActiveCell(scene, world) };
	std::optional<V2_int> anchor{ stroke_.has_last_cell ? std::optional<V2_int>{ stroke_.last_cell }
														: std::nullopt };
	V2_int current{ tool_ == PaintTool::Brush
						? ResolveStableBrushCenter(hovered_cell, brush_diameter_, anchor)
						: hovered_cell };
	if (!stroke_.has_last_cell) {
		stroke_.last_cell	  = current;
		stroke_.has_last_cell = true;
	}
	std::vector<V2_int> path{ LineCells(stroke_.last_cell, current) };

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
	V2_int first{ WorldToActiveCell(scene, a) };
	V2_int last{ WorldToActiveCell(scene, b) };
	auto cells{ LineCells(first, last) };
	for (V2_int cell : cells) {
		V2_int mn{ std::min(first.x, last.x), std::min(first.y, last.y) };
		V2_int mx{ std::max(first.x, last.x), std::max(first.y, last.y) };
		if (recipe_.operation != PaintBrushOperation::ExclusionMask &&
			!CoveragePass(cell, mn, mx)) {
			continue;
		}
		V2_float world{ ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f };
		std::size_t created_before{ stroke_.entities.created_roots.size() };
		ApplyAt(ctx, scene, world);
		if (line_align_rotation_ && stroke_.entities.created_roots.size() > created_before) {
			V2_float delta{ b - a };
			Radians angle{ std::atan2(delta.y, delta.x) };
			for (std::size_t i{ created_before }; i < stroke_.entities.created_roots.size(); ++i) {
				if (Entity e{ scene.GetEntity(stroke_.entities.created_roots[i]) }) {
					SetRotation(e, angle);
				}
			}
		}
	}
}

void PaintEditor::ApplyRectangle(EditorContext& ctx, Scene& scene, V2_float a, V2_float b) {
	V2_int first{ WorldToActiveCell(scene, a) };
	V2_int last{ WorldToActiveCell(scene, b) };
	for (V2_int cell : RectangleCells(first, last)) {
		V2_int mn{ std::min(first.x, last.x), std::min(first.y, last.y) };
		V2_int mx{ std::max(first.x, last.x), std::max(first.y, last.y) };
		if (recipe_.operation != PaintBrushOperation::ExclusionMask &&
			!CoveragePass(cell, mn, mx)) {
			continue;
		}
		ApplyAt(ctx, scene, ActiveCellToWorld(scene, cell) + ActiveGridSize(scene) * 0.5f);
	}
}

void PaintEditor::CreateGeneratorForStroke(
	EditorContext& ctx, Scene& scene, PaintGeneratorGeometry geometry
) {
	bool tile_mode{ IsTileMode(scene) };

	Tilemap target;
	bool created_target{};
	if (tile_mode) {
		target = ResolveTargetTilemap(scene);

		if (!target) {
			return;
		}
	}

	PaintGenerator generator{ CreatePaintGenerator(
		scene, Tag{ geometry == PaintGeneratorGeometry::BrushStroke ? "Brush Generator"
					: geometry == PaintGeneratorGeometry::Line		? "Line Generator"
																	: "Rectangle Generator" }
	) };
	if (!generator) {
		if (created_target && target) {
			target.Destroy();
			scene.Refresh();
			SetTargetTilemap(std::nullopt);
		}
		return;
	}

	auto& data{ generator.GetData() };
	data.geometry	 = geometry;
	data.recipe		 = CaptureGeneratorRecipe(tile_mode);
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
		std::uint32_t stroke_id{ next_active_brush_stroke_id_++ };
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

	if (target && !generator.SetTargetTilemap(target)) {
		generator.Destroy();
		if (created_target) {
			target.Destroy();
			SetTargetTilemap(std::nullopt);
		}
		scene.Refresh();
		return;
	}

	scene.Refresh();

	if (geometry == PaintGeneratorGeometry::BrushStroke) {
		// The first Brush stroke starts a live generator. Do not push Create Entity yet:
		// Cancel Generator must be able to discard the complete authoring session without
		// leaving a stale global undo entry.
		active_generator_before_selection_ = ctx.local.selection;
		active_brush_generator_			   = generator.Get<UUID>();
		active_generator_created_tilemap_ =
			created_target ? std::optional<UUID>{ target.Get<UUID>() } : std::nullopt;
		active_generator_undo_cursor_ = ctx.undo.Cursor();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(generator, false);
		return;
	}

	Entity record_root{ created_target ? Entity{ target } : Entity{ generator } };
	RecordPaintCreatedEntity(ctx, record_root, ctx.local.selection);
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(generator, false);
}

bool PaintEditor::IsActiveBrushGenerator(Entity generator) const {
	return generator && active_brush_generator_.has_value() && generator.Has<UUID>() &&
		   generator.Get<UUID>() == *active_brush_generator_;
}

void PaintEditor::AppendBrushStrokeToGenerator(EditorContext& ctx, Scene& scene) {
	bool tile_mode{ IsTileMode(scene) };

	Entity entity{ active_brush_generator_.has_value() ? scene.GetEntity(*active_brush_generator_)
													   : Entity{} };

	if (!entity || !IsPaintGenerator(entity) ||
		static_cast<bool>(PaintGenerator{ entity }.GetTargetTilemap()) != tile_mode) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		active_generator_undo_cursor_.reset();
		active_generator_created_tilemap_.reset();
		next_active_brush_stroke_id_ = 1;
		CreateGeneratorForStroke(ctx, scene, PaintGeneratorGeometry::BrushStroke);
		return;
	}

	PaintGenerator generator{ entity };
	auto& data{ generator.GetData() };

	// The generator owns its recipe and geometry settings after the first stroke.
	// Appending another stroke only extends its captured authoring raster; it must not
	// overwrite inspector edits with the current paint toolbar state.
	std::uint32_t stroke_id{ next_active_brush_stroke_id_++ };
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
		active_generator_undo_cursor_.reset();
		active_generator_created_tilemap_.reset();
		next_active_brush_stroke_id_ = 1;
		return;
	}

	EditorSelection before{ active_generator_before_selection_.value_or(ctx.local.selection) };
	EditorSelection current_selection{ ctx.local.selection };
	const auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
	bool generator_is_still_selected{ hierarchy.GetActiveTab() ==
										  SceneHierarchyTab::SceneHierarchy &&
									  hierarchy.GetSelectedEntity() == generator };

	Entity record_root{ generator };
	if (active_generator_created_tilemap_.has_value()) {
		Entity tilemap{ scene.GetEntity(*active_generator_created_tilemap_) };
		if (tilemap && IsTilemap(tilemap)) {
			record_root = tilemap;
		}
	}

	RecordPaintCreatedEntity(ctx, record_root, before);
	if (generator_is_still_selected) {
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(generator, false);
	} else {
		ApplyEditorSelection(ctx, current_selection);
	}

	active_brush_generator_.reset();
	active_generator_before_selection_.reset();
	active_generator_undo_cursor_.reset();
	active_generator_created_tilemap_.reset();
	next_active_brush_stroke_id_ = 1;
	ctx.local.state.is_dirty	 = true;
}

void PaintEditor::CancelActiveBrushGenerator(EditorContext& ctx, Scene& scene, bool undoable) {
	if (!active_brush_generator_.has_value()) {
		return;
	}

	Entity generator{ scene.GetEntity(*active_brush_generator_) };
	std::optional<UUID> created_tilemap{ active_generator_created_tilemap_ };

	if (!undoable || !generator || !IsPaintGenerator(generator)) {
		bool destroyed{};

		if (generator) {
			generator.Destroy();
			destroyed = true;
		}

		if (created_tilemap.has_value()) {
			if (Entity tilemap{ scene.GetEntity(*created_tilemap) }) {
				tilemap.Destroy();
				destroyed = true;
			}

			if (target_tilemap_ == created_tilemap) {
				target_tilemap_.reset();
			}
			if (selected_tilemap_ == created_tilemap) {
				selected_tilemap_.reset();
				selected_tile_cells_.clear();
			}
		}

		if (destroyed) {
			scene.Refresh();
		}

		if (active_generator_before_selection_.has_value()) {
			ApplyEditorSelection(ctx, *active_generator_before_selection_);
		} else {
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		}

		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		active_generator_undo_cursor_.reset();
		active_generator_created_tilemap_.reset();
		next_active_brush_stroke_id_ = 1;
		stroke_						 = {};
		return;
	}

	Entity snapshot_root{ generator };
	if (created_tilemap.has_value()) {
		Entity tilemap{ scene.GetEntity(*created_tilemap) };
		if (tilemap && IsTilemap(tilemap)) {
			snapshot_root = tilemap;
		}
	}

	UUID generator_uuid{ generator.Get<UUID>() };
	UUID snapshot_root_uuid{ snapshot_root.Get<UUID>() };
	SerializedEntity snapshot{ SerializeEntity(snapshot_root) };
	std::optional<UUID> snapshot_parent;
	std::optional<Transform> snapshot_world_transform;
	if (HasParent(snapshot_root)) {
		Entity parent{ GetParent(snapshot_root) };
		if (parent && parent.Has<UUID>()) {
			snapshot_parent = parent.Get<UUID>();
		}
		if (snapshot_root.Has<Transform>()) {
			snapshot_world_transform = GetWorldTransform(snapshot_root);
		}
	}

	EditorSelection editor_selection_before{ ctx.local.selection };
	std::optional<EditorSelection> authoring_selection_before{ active_generator_before_selection_ };
	std::optional<UUID> target_tilemap_before{ target_tilemap_ };
	std::vector<UUID> selected_entities_before{ selected_entities_ };
	std::optional<UUID> selected_generator_before{ selected_generator_ };
	std::optional<UUID> selected_tilemap_before{ selected_tilemap_ };
	std::vector<V2_int> selected_tile_cells_before{ selected_tile_cells_ };
	std::uint32_t next_stroke_id_before{ next_active_brush_stroke_id_ };
	std::optional<std::size_t> undo_cursor_before{ active_generator_undo_cursor_ };
	Stroke stroke_before{ stroke_ };

	CancelActiveBrushGenerator(ctx, scene, false);

	EditorSelection editor_selection_after{ ctx.local.selection };
	std::optional<UUID> target_tilemap_after{ target_tilemap_ };
	std::vector<UUID> selected_entities_after{ selected_entities_ };
	std::optional<UUID> selected_generator_after{ selected_generator_ };
	std::optional<UUID> selected_tilemap_after{ selected_tilemap_ };
	std::vector<V2_int> selected_tile_cells_after{ selected_tile_cells_ };

	Scene* scene_ptr{ &scene };
	PaintEditor* paint{ this };
	EditorContext* context{ &ctx };

	PushPaintUndo(
		ctx, scene, "Cancel Generator",
		[scene_ptr, paint, context, generator_uuid, created_tilemap, snapshot_root_uuid, snapshot,
		 snapshot_parent, snapshot_world_transform, editor_selection_before,
		 authoring_selection_before, target_tilemap_before, selected_entities_before,
		 selected_generator_before, selected_tilemap_before, selected_tile_cells_before,
		 next_stroke_id_before, undo_cursor_before, stroke_before]() mutable {
			if (!scene_ptr->GetEntity(snapshot_root_uuid)) {
				RestoreEntityTree(*scene_ptr, snapshot);
				scene_ptr->Refresh();
			}

			Entity restored_root{ scene_ptr->GetEntity(snapshot_root_uuid) };
			if (restored_root && snapshot_parent.has_value()) {
				if (Entity parent{ scene_ptr->GetEntity(*snapshot_parent) }) {
					SetParent(restored_root, parent);
					if (snapshot_world_transform.has_value() && restored_root.Has<Transform>()) {
						SetWorldTransform(restored_root, *snapshot_world_transform);
					}
				}
			}
			scene_ptr->Refresh();

			paint->target_tilemap_					  = target_tilemap_before;
			paint->selected_entities_				  = selected_entities_before;
			paint->selected_generator_				  = selected_generator_before;
			paint->selected_tilemap_				  = selected_tilemap_before;
			paint->selected_tile_cells_				  = selected_tile_cells_before;
			paint->active_brush_generator_			  = generator_uuid;
			paint->active_generator_before_selection_ = authoring_selection_before;
			paint->active_generator_created_tilemap_  = created_tilemap;
			paint->active_generator_undo_cursor_	  = undo_cursor_before;
			paint->next_active_brush_stroke_id_		  = next_stroke_id_before;
			paint->stroke_							  = stroke_before;
			ApplyEditorSelection(*context, editor_selection_before);
		},
		[scene_ptr, paint, context, generator_uuid, created_tilemap, editor_selection_after,
		 target_tilemap_after, selected_entities_after, selected_generator_after,
		 selected_tilemap_after, selected_tile_cells_after]() mutable {
			bool destroyed{};
			if (Entity active{ scene_ptr->GetEntity(generator_uuid) }) {
				active.Destroy();
				destroyed = true;
			}
			if (created_tilemap.has_value()) {
				if (Entity tilemap{ scene_ptr->GetEntity(*created_tilemap) }) {
					tilemap.Destroy();
					destroyed = true;
				}
			}
			if (destroyed) {
				scene_ptr->Refresh();
			}

			paint->target_tilemap_		= target_tilemap_after;
			paint->selected_entities_	= selected_entities_after;
			paint->selected_generator_	= selected_generator_after;
			paint->selected_tilemap_	= selected_tilemap_after;
			paint->selected_tile_cells_ = selected_tile_cells_after;
			paint->active_brush_generator_.reset();
			paint->active_generator_before_selection_.reset();
			paint->active_generator_undo_cursor_.reset();
			paint->active_generator_created_tilemap_.reset();
			paint->next_active_brush_stroke_id_ = 1;
			paint->stroke_						= {};
			ApplyEditorSelection(*context, editor_selection_after);
		}
	);

	ctx.local.state.is_dirty = true;
}

bool PaintEditor::UndoActiveBrushStroke(EditorContext& ctx, Scene& scene) {
	if (!active_brush_generator_.has_value()) {
		return false;
	}

	Entity entity{ scene.GetEntity(*active_brush_generator_) };
	if (!entity || !IsPaintGenerator(entity)) {
		active_brush_generator_.reset();
		active_generator_before_selection_.reset();
		active_generator_created_tilemap_.reset();
		active_generator_undo_cursor_.reset();
		next_active_brush_stroke_id_ = 1;
		return false;
	}

	auto& points{ PaintGenerator{ entity }.GetData().stroke_points };
	if (points.empty()) {
		CancelActiveBrushGenerator(ctx, scene, false);
		return true;
	}

	std::uint32_t last_id{};
	for (const auto& point : points) {
		last_id = std::max(last_id, point.stroke_id);
	}

	std::erase_if(points, [last_id](const PaintGeneratorStrokePoint& point) {
		return point.stroke_id == last_id;
	});

	if (points.empty()) {
		CancelActiveBrushGenerator(ctx, scene, false);
		return true;
	}

	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(entity, false);
	ctx.local.state.is_dirty = true;
	return true;
}

bool PaintEditor::CanUndoActiveBrushStroke(const Scene& scene, std::size_t undo_cursor) const {
	if (!active_brush_generator_.has_value() || !active_generator_undo_cursor_.has_value() ||
		undo_cursor > *active_generator_undo_cursor_) {
		return false;
	}

	Entity entity{ scene.GetEntity(*active_brush_generator_) };
	return entity && IsPaintGenerator(entity);
}

void PaintEditor::CreateInfiniteGenerator(EditorContext& ctx, Scene& scene) {
	bool tile_mode{ IsTileMode(scene) };

	Tilemap target;
	bool created_target{};
	if (tile_mode) {
		target = ResolveTargetTilemap(scene);

		if (!target) {
			return;
		}
	}

	PaintGenerator generator{ CreatePaintGenerator(scene, Tag{ "Infinite Generator" }) };
	if (!generator) {
		if (created_target && target) {
			target.Destroy();
			scene.Refresh();
			SetTargetTilemap(std::nullopt);
		}
		return;
	}

	auto& data{ generator.GetData() };
	data.geometry	 = PaintGeneratorGeometry::Infinite;
	data.recipe		 = CaptureGeneratorRecipe(tile_mode);
	data.grid_size	 = ActiveGridSize(scene);
	data.grid_offset = ActiveGridOrigin(scene);

	if (target && !generator.SetTargetTilemap(target)) {
		generator.Destroy();
		if (created_target) {
			target.Destroy();
			SetTargetTilemap(std::nullopt);
		}
		scene.Refresh();
		return;
	}

	scene.Refresh();
	Entity record_root{ created_target ? Entity{ target } : Entity{ generator } };
	RecordPaintCreatedEntity(ctx, record_root, ctx.local.selection);
	ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(generator, false);
}

void PaintEditor::CommitTileStroke(EditorContext& ctx, Scene& scene) {
	std::optional<UUID> map_uuid;
	std::optional<::ptgn::impl::TilemapData> map_before;
	std::optional<::ptgn::impl::TilemapData> map_after;

	if (stroke_.tilemap && stroke_.tilemap_before) {
		if (Entity entity{ scene.GetEntity(*stroke_.tilemap) };
			entity && entity.Has<::ptgn::impl::TilemapData>()) {
			auto after{ entity.Get<::ptgn::impl::TilemapData>() };
			json before_json = *stroke_.tilemap_before;
			json after_json	 = after;
			if (before_json != after_json) {
				map_uuid   = *stroke_.tilemap;
				map_before = *stroke_.tilemap_before;
				map_after  = after;
			}
		}
	}

	auto generator_before{ stroke_.generator_before };
	std::vector<std::pair<UUID, ::ptgn::impl::PaintGeneratorData>> generator_after;
	for (const auto& [uuid, _before] : generator_before) {
		if (Entity entity{ scene.GetEntity(uuid) }; entity && IsPaintGenerator(entity)) {
			generator_after.emplace_back(uuid, entity.Get<::ptgn::impl::PaintGeneratorData>());
		}
	}

	bool created_tilemap{ stroke_.created_tilemap && map_uuid.has_value() };
	if (stroke_.created_tilemap && !map_uuid.has_value()) {
		if (stroke_.tilemap.has_value()) {
			if (Entity map{ scene.GetEntity(*stroke_.tilemap) }) {
				map.Destroy();
				scene.Refresh();
			}
		}
		target_tilemap_ = stroke_.previous_target_tilemap;
	}

	if (!map_uuid && generator_before.empty()) {
		return;
	}

	std::optional<SerializedEntity> created_snapshot;
	if (created_tilemap) {
		Entity map{ scene.GetEntity(*map_uuid) };
		created_snapshot = SerializeEntity(map);
	}

	std::optional<UUID> previous_target_tilemap{ stroke_.previous_target_tilemap };
	Scene* scene_ptr{ &scene };
	PaintEditor* paint{ this };
	auto apply_generators =
		[scene_ptr](const std::vector<std::pair<UUID, ::ptgn::impl::PaintGeneratorData>>& states) {
			for (const auto& [uuid, data] : states) {
				if (Entity entity{ scene_ptr->GetEntity(uuid) };
					entity && IsPaintGenerator(entity)) {
					entity.Get<::ptgn::impl::PaintGeneratorData>() = data;
				}
			}
		};

	PushPaintUndo(
		ctx, scene, tool_ == PaintTool::Erase ? "Erase Paint" : "Paint Tiles",
		[scene_ptr, paint, map_uuid, map_before, created_tilemap, previous_target_tilemap,
		 generator_before, apply_generators]() mutable {
			if (created_tilemap && map_uuid) {
				if (Entity map{ scene_ptr->GetEntity(*map_uuid) }) {
					map.Destroy();
					scene_ptr->Refresh();
				}
				if (paint->target_tilemap_ == map_uuid) {
					paint->target_tilemap_ = previous_target_tilemap;
				}
				if (paint->selected_tilemap_ == map_uuid) {
					paint->selected_tilemap_.reset();
					paint->selected_tile_cells_.clear();
				}
			} else if (map_uuid && map_before) {
				if (Entity entity{ scene_ptr->GetEntity(*map_uuid) };
					entity && entity.Has<::ptgn::impl::TilemapData>()) {
					entity.Get<::ptgn::impl::TilemapData>() = *map_before;
				}
			}
			apply_generators(generator_before);
		},
		[scene_ptr, paint, map_uuid, map_after, created_tilemap, created_snapshot, generator_after,
		 apply_generators]() mutable {
			if (created_tilemap && map_uuid && created_snapshot) {
				if (!scene_ptr->GetEntity(*map_uuid)) {
					RestoreEntityTree(*scene_ptr, *created_snapshot);
					scene_ptr->Refresh();
				}
				paint->target_tilemap_ = map_uuid;
			} else if (map_uuid && map_after) {
				if (Entity entity{ scene_ptr->GetEntity(*map_uuid) };
					entity && entity.Has<::ptgn::impl::TilemapData>()) {
					entity.Get<::ptgn::impl::TilemapData>() = *map_after;
				}
			}
			apply_generators(generator_after);
		}
	);
}

void PaintEditor::CommitEntityStroke(EditorContext& ctx, Scene& scene) {
	if (stroke_.entity_mask_capture) {
		auto before{ stroke_.entity_mask_before };
		std::vector<V2_int> after{ stroke_.entity_mask_cells.begin(),
								   stroke_.entity_mask_cells.end() };
		SetEntityExclusionCells(ctx.project_state.paint, scene.GetTag(), std::move(after));
		after = GetEntityExclusionCells(ctx.project_state.paint, scene.GetTag());
		if (before != after) {
			EditorContext* context{ &ctx };
			std::string scene_key{ scene.GetTag() };
			PushPaintUndo(
				ctx, scene,
				tool_ == PaintTool::Erase ? "Erase Exclusion Mask" : "Paint Exclusion Mask",
				[context, scene_key, before]() {
					SetEntityExclusionCells(context->project_state.paint, scene_key, before);
				},
				[context, scene_key, after]() {
					SetEntityExclusionCells(context->project_state.paint, scene_key, after);
				}
			);
		}
		return;
	}
	std::vector<DeletedEntity> created;
	created.reserve(stroke_.entities.created_roots.size());
	for (UUID uuid : stroke_.entities.created_roots) {
		if (Entity entity{ scene.GetEntity(uuid) }) {
			created.push_back(DeletedEntity{ .entity = SerializeEntity(entity) });
		}
	}
	auto deleted{ stroke_.entities.deleted_roots };

	auto generator_before{ stroke_.generator_before };
	std::vector<std::pair<UUID, ::ptgn::impl::PaintGeneratorData>> generator_after;
	for (const auto& [uuid, _before] : generator_before) {
		if (Entity entity{ scene.GetEntity(uuid) }; entity && IsPaintGenerator(entity)) {
			generator_after.emplace_back(uuid, entity.Get<::ptgn::impl::PaintGeneratorData>());
		}
	}

	if (created.empty() && deleted.empty() && generator_before.empty()) {
		return;
	}

	Scene* scene_ptr{ &scene };

	auto apply_entities = [scene_ptr](
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
			RestoreEntityTree(*scene_ptr, item.entity);
		}
		scene_ptr->Refresh();
	};

	auto apply_generators =
		[scene_ptr](const std::vector<std::pair<UUID, ::ptgn::impl::PaintGeneratorData>>& states) {
			for (const auto& [uuid, data] : states) {
				if (Entity entity{ scene_ptr->GetEntity(uuid) };
					entity && IsPaintGenerator(entity)) {
					entity.Get<::ptgn::impl::PaintGeneratorData>() = data;
				}
			}
		};

	PushPaintUndo(
		ctx, scene, tool_ == PaintTool::Erase ? "Erase Paint" : "Paint Entities",
		[apply_entities, apply_generators, created, deleted, generator_before]() mutable {
			apply_entities(created, deleted);
			apply_generators(generator_before);
		},
		[apply_entities, apply_generators, created, deleted, generator_after]() mutable {
			apply_entities(deleted, created);
			apply_generators(generator_after);
		}
	);
}

void PaintEditor::EndStroke(EditorContext& ctx, Scene& scene, V2_float world) {
	if (!stroke_.active) {
		return;
	}
	stroke_.current_world = world;
	bool keep_generator{ recipe_.commit_mode == PaintCommitMode::KeepGenerator &&
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
		V2_int anchor_cell{ stroke_.last_cell };
		bool preserve_anchor{ tool_ == PaintTool::Brush };
		stroke_ = {};
		if (preserve_anchor) {
			stroke_.last_cell	  = anchor_cell;
			stroke_.has_last_cell = true;
		}
		return;
	}
	if (tool_ == PaintTool::Line) {
		ApplyLine(ctx, scene, stroke_.start_world, world);
	}
	if (tool_ == PaintTool::Rectangle) {
		ApplyRectangle(ctx, scene, stroke_.start_world, world);
	}
	if (IsTileMode(scene)) {
		CommitTileStroke(ctx, scene);
	} else {
		CommitEntityStroke(ctx, scene);
	}
	V2_int anchor_cell{ stroke_.last_cell };
	bool preserve_anchor{ tool_ == PaintTool::Brush || tool_ == PaintTool::Erase };
	stroke_ = {};
	if (preserve_anchor) {
		stroke_.last_cell	  = anchor_cell;
		stroke_.has_last_cell = true;
	}
}

void PaintEditor::CancelStroke(EditorContext&, Scene& scene) {
	if (!stroke_.active) {
		return;
	}
	if (stroke_.created_tilemap && stroke_.tilemap.has_value()) {
		if (Entity map{ scene.GetEntity(*stroke_.tilemap) }) {
			map.Destroy();
			scene.Refresh();
		}
		target_tilemap_ = stroke_.previous_target_tilemap;
	} else if (stroke_.tilemap.has_value() && stroke_.tilemap_before.has_value()) {
		if (Entity map{ scene.GetEntity(*stroke_.tilemap) };
			map && map.Has<::ptgn::impl::TilemapData>()) {
			map.Get<::ptgn::impl::TilemapData>() = *stroke_.tilemap_before;
		}
	}
	for (const auto& [uuid, before] : stroke_.generator_before) {
		if (Entity entity{ scene.GetEntity(uuid) }; entity && IsPaintGenerator(entity)) {
			entity.Get<::ptgn::impl::PaintGeneratorData>() = before;
		}
	}
	for (UUID uuid : stroke_.entities.created_roots) {
		if (Entity e{ scene.GetEntity(uuid) }) {
			e.Destroy();
		}
	}
	scene.Refresh();
	for (const auto& deleted : stroke_.entities.deleted_roots) {
		RestoreEntityTree(scene, deleted.entity);
	}
	scene.Refresh();
	V2_int anchor_cell{ stroke_.last_cell };
	bool preserve_anchor{ tool_ == PaintTool::Brush || tool_ == PaintTool::Erase };
	stroke_ = {};
	if (preserve_anchor) {
		stroke_.last_cell	  = anchor_cell;
		stroke_.has_last_cell = true;
	}
}

void PaintEditor::SelectClick(
	EditorContext& ctx, Scene& scene, V2_float world, bool additive, bool toggle, Entity picked_ui
) {
	// Select the frontmost scene entity or tile under the cursor, ordered by depth.
	// Painting still targets only the explicitly selected Tilemap.
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

	// Prefer the actual visible pixel under the cursor for UI, including fixed-camera
	// UI elements. World-space root bounds cannot distinguish a stack from its children.
	if (picked_ui && !picked_ui.HasAny<GroupStackData, Group>()) {
		hit.kind   = Hit::Kind::Entity;
		hit.entity = picked_ui;
	}

	// Select from ordinary scene entities, ordered by their depth, regardless of type.
	auto roots{ SceneRootEntities(scene) };
	for (auto it{ roots.rbegin() }; it != roots.rend() && hit.kind == Hit::Kind::None; ++it) {
		Entity root{ *it };
		if (!root || root.HasAny<GroupStackData, Group>() || IsProtectedSceneEntity(scene, root) ||
			IsPaintGenerator(root)) {
			continue;
		}
		if (IsTilemap(root)) {
			Tilemap map{ root };
			const auto& tiles{ map.GetData().tiles };
			for (auto tile_it{ tiles.rbegin() }; tile_it != tiles.rend(); ++tile_it) {
				if (Contains(TileSelectionBounds(map, *tile_it), world)) {
					hit.kind	  = Hit::Kind::Tile;
					hit.tilemap	  = map;
					hit.tile_cell = tile_it->coordinate;
					break;
				}
			}
		} else if (
			auto bounds{ EntitySelectionBounds(root) };
			bounds.has_value() && Contains(*bounds, world)
		) {
			hit.kind   = Hit::Kind::Entity;
			hit.entity = root;
		}
	}
	if (hit.kind == Hit::Kind::None) {
		if (Entity generator{ FindGeneratorAtWorld(scene, world) }) {
			hit.kind   = Hit::Kind::Generator;
			hit.entity = generator;
		}
	}

	switch (hit.kind) {
		case Hit::Kind::Generator: {
			UUID uuid{ hit.entity.Get<UUID>() };
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

			// Tile multiple selection is intentionally scoped to one Tilemap. Clicking
			// a tile in another Tilemap starts a fresh tile selection, while Shift/
			// Ctrl continue to work within the same Tilemap.
			UUID map_uuid{ hit.tilemap.Get<UUID>() };
			if (!selected_tilemap_.has_value() || *selected_tilemap_ != map_uuid) {
				selected_tile_cells_.clear();
			}

			selected_entities_.clear();
			selected_generator_.reset();
			ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(hit.tilemap, false);
			selected_tilemap_ = map_uuid;

			auto it{ std::ranges::find(selected_tile_cells_, hit.tile_cell) };
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

			UUID uuid{ hit.entity.Get<UUID>() };
			auto it{ std::ranges::find(selected_entities_, uuid) };
			if (toggle && it != selected_entities_.end()) {
				selected_entities_.erase(it);

				Entity primary{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
				if (primary && primary.Get<UUID>() == uuid) {
					if (!selected_entities_.empty()) {
						ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(
							scene.GetEntity(selected_entities_.back()), false
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
	PaintSelectionRect selection{ MakeSelectionRect(a, b) };

	// Use scene entity depth, not organizational layers, for marquee priority.
	struct TileMarqueeHit {
		Tilemap map{};
		std::vector<V2_int> cells{};
		std::size_t depth_index{};
	};

	std::optional<TileMarqueeHit> tile_hit;
	std::optional<std::size_t> top_entity_depth;
	auto roots{ SceneRootEntities(scene) };
	for (std::size_t i{ roots.size() }; i > 0; --i) {
		std::size_t depth_index{ i - 1 };
		Entity root{ roots[depth_index] };
		if (!root || root.HasAny<GroupStackData, Group>() || IsProtectedSceneEntity(scene, root) ||
			IsPaintGenerator(root)) {
			continue;
		}
		if (IsTilemap(root)) {
			if (tile_hit) {
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
				tile_hit = TileMarqueeHit{ .map			= map,
										   .cells		= std::move(cells),
										   .depth_index = depth_index };
			}
		} else if (!top_entity_depth) {
			if (auto bounds{ EntitySelectionBounds(root) };
				bounds.has_value() && Overlaps(selection, *bounds)) {
				top_entity_depth = depth_index;
			}
		}
	}
	bool select_tiles{ tile_hit.has_value() &&
					   (!top_entity_depth || tile_hit->depth_index > *top_entity_depth ||
						target_tilemap_ == std::optional<UUID>{ tile_hit->map.Get<UUID>() }) };

	if (select_tiles) {
		UUID map_uuid{ tile_hit->map.Get<UUID>() };

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
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(tile_hit->map, false);
		selected_tilemap_ = map_uuid;

		for (V2_int cell : tile_hit->cells) {
			auto it{ std::ranges::find(selected_tile_cells_, cell) };
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

	// Select entities scene wide when no Tilemap has priority at the marquee bounds.
	if (!additive && !toggle) {
		selected_entities_.clear();
	}
	selected_generator_.reset();
	selected_tilemap_.reset();
	selected_tile_cells_.clear();

	Entity primary{};
	for (Entity root : roots) {
		if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
			root.HasAny<GroupStackData, Group>() || IsProtectedSceneEntity(scene, root)) {
			continue;
		}
		auto bounds{ EntitySelectionBounds(root) };
		if (!bounds.has_value() || !Overlaps(selection, *bounds)) {
			continue;
		}
		UUID uuid{ root.Get<UUID>() };
		auto it{ std::ranges::find(selected_entities_, uuid) };
		if (toggle && it != selected_entities_.end()) {
			selected_entities_.erase(it);
		} else {
			if (it == selected_entities_.end()) {
				selected_entities_.push_back(uuid);
			}
			primary = root;
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
	bool tile_mode{ IsTileMode(scene) };

	std::optional<V2_int> anchor{ stroke_.has_last_cell ? std::optional<V2_int>{ stroke_.last_cell }
														: std::nullopt };
	V2_int center{
		ResolveStableBrushCenter(WorldToActiveCell(scene, world), selection_diameter_, anchor)
	};
	stroke_.last_cell	  = center;
	stroke_.has_last_cell = true;
	auto brush_cells{ SelectionBrushCells(center) };
	V2_float grid_size{ ActiveGridSize(scene) };
	auto brush_rect = [&](V2_int cell) {
		V2_float min{ ActiveCellToWorld(scene, cell) };
		return PaintSelectionRect{ .min = min, .max = min + grid_size };
	};

	if (tile_mode) {
		Tilemap map{ ResolveTargetTilemap(scene) };
		if (!map) {
			return;
		}
		selected_entities_.clear();
		selected_generator_.reset();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity(map, false);
		selected_tilemap_ = map.Get<UUID>();

		for (const TilemapTile& tile : map.GetData().tiles) {
			PaintSelectionRect tile_bounds{ TileSelectionBounds(map, tile) };
			bool overlaps{ std::ranges::any_of(brush_cells, [&](V2_int cell) {
				return Overlaps(tile_bounds, brush_rect(cell));
			}) };
			if (!overlaps) {
				continue;
			}
			auto it{ std::ranges::find(selected_tile_cells_, tile.coordinate) };
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
	for (Entity root : SceneRootEntities(scene)) {
		if (!root || IsPaintGenerator(root) || IsTilemap(root) ||
			root.HasAny<GroupStackData, Group>() || IsProtectedSceneEntity(scene, root)) {
			continue;
		}
		auto bounds{ EntitySelectionBounds(root) };
		if (!bounds.has_value()) {
			continue;
		}
		bool overlaps{ std::ranges::any_of(brush_cells, [&](V2_int cell) {
			return Overlaps(*bounds, brush_rect(cell));
		}) };
		if (!overlaps) {
			continue;
		}
		UUID uuid{ root.Get<UUID>() };
		auto it{ std::ranges::find(selected_entities_, uuid) };
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
		if (auto bounds{ GeneratorSelectionBounds(selected) };
			bounds.has_value() && Contains(*bounds, world)) {
			return true;
		}
	}

	for (UUID uuid : selected_entities_) {
		Entity entity{ scene.GetEntity(uuid) };
		if (entity) {
			auto bounds{ EntitySelectionBounds(entity) };
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
	std::vector<V2_int> selected_cells_before{ selected_tile_cells_ };
	std::vector<V2_int> selected_cells_after{ selected_tile_cells_ };

	auto snap_axis = [](float value, float origin, float step) {
		step = std::max(1.0f, step);
		return origin + std::round((value - origin) / step) * step;
	};

	if (selected_generator_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_generator_) };
		if (entity && IsPaintGenerator(entity) && entity.Has<Transform>()) {
			{
				V2_float size{ entity_grid_size_ };
				V2_float origin{ entity_grid_offset_ };
				if (Tilemap grid_map{ PaintGenerator{ entity }.GetTargetTilemap() }) {
					size   = grid_map.GetData().cell_size;
					origin = GetWorldPosition(grid_map);
				}
				Transform before{ GetWorldTransform(entity) };
				Transform after{ before };
				V2_float generator_grid_origin{ GetWorldPosition(entity) +
												PaintGenerator{ entity }.GetData().grid_offset };
				V2_float snapped_grid_origin{
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
		V2_float size{ entity_grid_size_ };
		V2_float origin{ entity_grid_offset_ };
		for (UUID uuid : selected_entities_) {
			Entity entity{ scene.GetEntity(uuid) };
			if (!entity || !entity.Has<Transform>() || IsProtectedSceneEntity(scene, entity)) {
				continue;
			}

			Transform before{ GetWorldTransform(entity) };
			Transform after{ before };
			Origin draw_origin{ entity.GetOrDefault<Origin>() };
			V2_float anchor_offset{ size * 0.5f - GetOffset(draw_origin, size) };
			after.position = {
				origin.x +
					std::round(
						(before.position.x - origin.x - anchor_offset.x) / std::max(1.0f, size.x)
					) * std::max(1.0f, size.x) +
					anchor_offset.x,
				origin.y +
					std::round(
						(before.position.y - origin.y - anchor_offset.y) / std::max(1.0f, size.y)
					) * std::max(1.0f, size.y) +
					anchor_offset.y,
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
			{
				Tilemap map{ entity };
				tilemap_uuid   = map.Get<UUID>();
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
				V2_float cell_size{ map.GetData().cell_size };
				V2_float map_origin{ GetWorldPosition(map) };
				for (TilemapTile tile : moved) {
					V2_float anchor{ map.CellToWorld(tile.coordinate) + tile.offset };
					V2_int target{
						static_cast<int>(
							std::lround((anchor.x - map_origin.x) / std::max(1.0f, cell_size.x))
						),
						static_cast<int>(
							std::lround((anchor.y - map_origin.y) / std::max(1.0f, cell_size.y))
						),
					};
					tile.coordinate = target;
					tile.offset		= {};
					map.SetTile(std::move(tile));
					if (!std::ranges::contains(selected_tile_cells_, target)) {
						selected_tile_cells_.push_back(target);
					}
				}
				tilemap_after		 = map.GetData();
				selected_cells_after = selected_tile_cells_;
			}
		}
	}

	bool changed{ !entity_before.empty() };
	if (tilemap_before.has_value() && tilemap_after.has_value()) {
		json before_json = *tilemap_before;
		json after_json	 = *tilemap_after;
		changed			 = changed || before_json != after_json;
	}
	if (!changed) {
		return;
	}

	Scene* scene_ptr{ &scene };
	PaintEditor* paint{ this };
	PushPaintUndo(
		ctx, scene, "Snap Selection to Grid",
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
					paint->selected_tile_cells_				= selected_cells_before;
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
					paint->selected_tile_cells_				= selected_cells_after;
				}
			}
		}
	);
}

void PaintEditor::BeginMove(EditorContext&, Scene& scene, V2_float world) {
	if (!HasSelection()) {
		return;
	}

	move_					= {};
	move_.start_mouse_world = world;

	if (selected_generator_.has_value()) {
		Entity entity{ scene.GetEntity(*selected_generator_) };
		if (!entity || !IsPaintGenerator(entity) || !entity.Has<Transform>()) {
			return;
		}
		move_.entity_before.emplace_back(entity.Get<UUID>(), CaptureEditorEntityTransform(entity));
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
			move_.entity_before.emplace_back(uuid, CaptureEditorEntityTransform(entity));
		}
		// Apply parent transforms before their selected children. Otherwise moving both
		// can translate a child twice, depending on the selection iteration order.
		std::stable_sort(
			move_.entity_before.begin(), move_.entity_before.end(),
			[&scene](const auto& a, const auto& b) {
				auto depth = [&scene](UUID uuid) {
					std::size_t count{ 0 };
					Entity entity{ scene.GetEntity(uuid) };
					while (entity && HasParent(entity)) {
						++count;
						entity = GetParent(entity);
					}
					return count;
				};
				return depth(a.first) < depth(b.first);
			}
		);
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
	Tilemap map{ entity };
	move_.tilemap		 = map.Get<UUID>();
	move_.tile_cells	 = selected_tile_cells_;
	move_.tilemap_before = map.GetData();
	move_.active		 = true;
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
		if (Entity entity{ scene.GetEntity(*selected_generator_) };
			entity && IsPaintGenerator(entity)) {
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
				MoveEditorEntityFromSnapshot(entity, before, delta);
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
		V2_int cell_delta{
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
			V2_int target{ tile.coordinate + cell_delta };
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
		std::vector<std::pair<UUID, EditorEntityTransformSnapshot>> after;
		after.reserve(move_.entity_before.size());
		bool changed{ false };
		for (const auto& [uuid, before] : move_.entity_before) {
			if (Entity entity{ scene.GetEntity(uuid) }) {
				auto snapshot{ CaptureEditorEntityTransform(entity) };
				changed |= snapshot.world != before.world || snapshot.local != before.local;
				after.emplace_back(uuid, std::move(snapshot));
			}
		}
		if (changed) {
			Scene* scene_ptr{ &scene };
			auto before{ move_.entity_before };
			PushPaintUndo(
				ctx, scene, "Move Selection",
				[scene_ptr, before]() {
					for (const auto& [uuid, snapshot] : before) {
						if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
							ApplyEditorEntityTransform(entity, snapshot);
						}
					}
				},
				[scene_ptr, after]() {
					for (const auto& [uuid, snapshot] : after) {
						if (Entity entity{ scene_ptr->GetEntity(uuid) }) {
							ApplyEditorEntityTransform(entity, snapshot);
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
				UUID uuid{ *move_.tilemap };
				auto before_cells{ move_.tile_cells };
				auto after_cells{ selected_tile_cells_ };
				PushPaintUndo(
					ctx, scene, "Move Selection",
					[scene_ptr, paint, uuid, before, before_cells]() {
						if (Entity e{ scene_ptr->GetEntity(uuid) };
							e && e.Has<::ptgn::impl::TilemapData>()) {
							e.Get<::ptgn::impl::TilemapData>() = before;
							paint->selected_tile_cells_		   = before_cells;
						}
					},
					[scene_ptr, paint, uuid, after, after_cells]() {
						if (Entity e{ scene_ptr->GetEntity(uuid) };
							e && e.Has<::ptgn::impl::TilemapData>()) {
							e.Get<::ptgn::impl::TilemapData>() = after;
							paint->selected_tile_cells_		   = after_cells;
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
	for (const auto& [uuid, snapshot] : move_.entity_before) {
		if (Entity entity{ scene.GetEntity(uuid) }) {
			ApplyEditorEntityTransform(entity, snapshot);
		}
	}
	if (move_.tilemap.has_value() && move_.tilemap_before.has_value()) {
		if (Entity entity{ scene.GetEntity(*move_.tilemap) };
			entity && entity.Has<::ptgn::impl::TilemapData>()) {
			entity.Get<::ptgn::impl::TilemapData>() = *move_.tilemap_before;
		}
	}
	selected_tile_cells_ = move_.tile_cells;
	move_				 = {};
}

Entity PaintEditor::FindGeneratorAtWorld(Scene& scene, V2_float world) const {
	// Explicit scene entity hit testing: tiles and manually placed entities block generators.
	auto roots{ SceneRootEntities(scene) };
	for (auto it{ roots.rbegin() }; it != roots.rend(); ++it) {
		Entity root{ *it };
		if (!root || IsPaintGenerator(root) || IsProtectedSceneEntity(scene, root)) {
			continue;
		}
		if (IsTilemap(root)) {
			Tilemap map{ root };
			for (const TilemapTile& tile : map.GetData().tiles) {
				if (Contains(TileSelectionBounds(map, tile), world)) {
					return {};
				}
			}
		} else if (!root.HasAny<GroupStackData, Group>()) {
			if (auto bounds{ EntitySelectionBounds(root) };
				bounds.has_value() && Contains(*bounds, world)) {
				return {};
			}
		}
	}
	std::vector<Entity> generators;
	for (Entity entity : scene.Entities()) {
		if (IsPaintGenerator(entity)) {
			generators.emplace_back(entity);
		}
	}
	std::ranges::sort(generators, [](Entity a, Entity b) {
		float da{ a.Has<Depth>() ? a.Get<Depth>().value : 0.0f };
		float db{ b.Has<Depth>() ? b.Get<Depth>().value : 0.0f };
		return da < db;
	});
	for (auto it{ generators.rbegin() }; it != generators.rend(); ++it) {
		if (GeneratorContainsWorld(*it, world)) {
			return *it;
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
	bool tile_mode{ IsTileMode(scene) };

	if (active_brush_generator_.has_value()) {
		Entity active{ scene.GetEntity(*active_brush_generator_) };
		const auto& hierarchy{ ctx.editor.GetSceneHierarchyPanel() };
		bool active_is_selected{ hierarchy.GetActiveTab() == SceneHierarchyTab::SceneHierarchy &&
								 hierarchy.GetSelectedEntity() == active };
		bool still_compatible{ active && IsPaintGenerator(active) && active_is_selected &&
							   tool_ == PaintTool::Brush &&
							   recipe_.commit_mode == PaintCommitMode::KeepGenerator &&
							   recipe_.operation == PaintBrushOperation::Paint &&
							   static_cast<bool>(PaintGenerator{ active }.GetTargetTilemap()) ==
								   tile_mode };
		if (!still_compatible) {
			FinishActiveBrushGenerator(ctx, scene);
		}
	}

	V2_float mouse_screen{ FromImGui(ImGui::GetIO().MousePos) };
	bool inside{ Contains(image_viewport, mouse_screen) };
	V2_float mouse_world{ ScreenToWorld(mouse_screen, frame, presentation_viewport) };
	if (inside && !stroke_.active) {
		bool selection_brush{ tool_ == PaintTool::Select &&
							  select_mode_ == PaintSelectMode::Brush };
		if (selection_brush || tool_ == PaintTool::Brush || tool_ == PaintTool::Erase) {
			std::optional<V2_int> anchor{ stroke_.has_last_cell
											  ? std::optional<V2_int>{ stroke_.last_cell }
											  : std::nullopt };
			int diameter{ selection_brush ? selection_diameter_ : brush_diameter_ };
			stroke_.last_cell =
				ResolveStableBrushCenter(WorldToActiveCell(scene, mouse_world), diameter, anchor);
			stroke_.has_last_cell = true;
		}
	}
	if (stroke_.active && inside) {
		// Keep the pending generator preview on the current cursor this frame rather than
		// waiting for the input update below. UpdateStroke will commit the same point later.
		stroke_.current_world = mouse_world;
	}

	ImDrawList* draw{ ImGui::GetWindowDrawList() };
	DrawTilemaps(ctx, scene, draw, presentation_viewport, frame);
	if (stroke_.entity_mask_capture) {
		for (V2_int cell : stroke_.entity_mask_cells) {
			V2_float mn{ entity_grid_offset_ +
						 V2_float{ static_cast<float>(cell.x), static_cast<float>(cell.y) } *
							 entity_grid_size_ };
			V2_float mx{ mn + entity_grid_size_ };
			ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
			ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };
			draw->AddRectFilled(p0, p1, IM_COL32(255, 26, 26, 46));
			draw->AddRect(p0, p1, IM_COL32(255, 72, 72, 120));
		}
	}
	for (const auto& mask : ctx.project_state.paint.entity_exclusion_masks) {
		if (mask.scene_key != scene.GetTag() || stroke_.entity_mask_capture) {
			continue;
		}
		for (V2_int cell : mask.cells) {
			V2_float mn{ entity_grid_offset_ +
						 V2_float{ static_cast<float>(cell.x), static_cast<float>(cell.y) } *
							 entity_grid_size_ };
			V2_float mx{ mn + entity_grid_size_ };
			ImVec2 p0{ ToImGui(WorldToScreen(mn, frame, presentation_viewport)) };
			ImVec2 p1{ ToImGui(WorldToScreen(mx, frame, presentation_viewport)) };
			draw->AddRectFilled(p0, p1, IM_COL32(255, 26, 26, 46));
			draw->AddRect(p0, p1, IM_COL32(255, 72, 72, 120));
		}
	}
	DrawGenerators(ctx, scene, draw, image_viewport, presentation_viewport, frame);
	DrawActiveGeneratorPreview(ctx, scene, draw, presentation_viewport, frame);
	DrawGrid(scene, draw, image_viewport, presentation_viewport, frame);
	DrawSelectionOverlay(scene, draw, presentation_viewport, frame);

	if (inside) {
		DrawToolPreview(scene, draw, mouse_world, presentation_viewport, frame);
	}
	bool pointer_operation_active{ stroke_.active || move_.active || selection_drag_active_ };
	if ((!ctx.local.state.viewport.hovered && !pointer_operation_active) ||
		ImGui::GetIO().WantTextInput) {
		return false;
	}

	if (!pointer_operation_active && ImGui::IsKeyPressed(ImGuiKey_Delete, false) &&
		DeleteSelection(ctx, scene)) {
		return true;
	}

	HandleShortcuts(ctx, scene);

	if (active_brush_generator_.has_value() && !stroke_.active &&
		ImGui::IsKeyPressed(ImGuiKey_Enter, false)) {
		FinishActiveBrushGenerator(ctx, scene);
		return true;
	}

	if (tool_ == PaintTool::Select && !active_brush_generator_.has_value() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		selection_drag_start_  = {};
		selection_drag_active_ = false;
		ClearSelection();
		ctx.editor.GetSceneHierarchyPanel().SetSelectedEntity({}, false);
		return true;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) ||
		ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
		bool had_active_operation{ stroke_.active || move_.active || selection_drag_active_ ||
								   active_brush_generator_.has_value() };
		if (active_brush_generator_.has_value() && !stroke_.active) {
			CancelActiveBrushGenerator(ctx, scene);
			return true;
		}
		if (stroke_.active) {
			CancelStroke(ctx, scene);
		}
		if (move_.active) {
			CancelMove(scene);
		}
		selection_drag_start_  = {};
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
			bool additive{ ImGui::GetIO().KeyShift };
			bool toggle{ CtrlDown() };
			Entity picked_ui{ PickUIContent(ctx, scene, mouse_screen, image_viewport) };
			bool picked_different_ui{
				picked_ui && !std::ranges::contains(selected_entities_, picked_ui.Get<UUID>())
			};

			if (picked_different_ui || !SelectionHitAtWorld(scene, mouse_world)) {
				SelectClick(ctx, scene, mouse_world, additive, toggle, picked_ui);
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
			selection_drag_start_  = {};
			selection_drag_active_ = false;
			if (inside && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
				SelectBrush(ctx, scene, mouse_world, CtrlDown());
				return true;
			}
			return false;
		}

		if (inside && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
			selection_drag_start_  = mouse_world;
			selection_drag_active_ = true;
			return true;
		}

		if (selection_drag_active_ && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			V2_float selection_end{ mouse_world };
			bool additive{ ImGui::GetIO().KeyShift };
			bool toggle{ CtrlDown() };

			V2_float start_screen{
				WorldToScreen(selection_drag_start_, frame, presentation_viewport)
			};
			V2_float end_screen{ WorldToScreen(mouse_world, frame, presentation_viewport) };
			V2_float screen_delta{ end_screen - start_screen };
			float screen_distance{
				std::sqrt(screen_delta.x * screen_delta.x + screen_delta.y * screen_delta.y)
			};

			if (screen_distance < 4.0f) {
				SelectClick(
					ctx, scene, mouse_world, additive, toggle,
					PickUIContent(ctx, scene, mouse_screen, image_viewport)
				);
			} else {
				if (additive) {
					selection_end = ConstrainSquareDrag(selection_drag_start_, mouse_world);
				}
				SelectMarquee(ctx, scene, selection_drag_start_, selection_end, additive, toggle);
			}

			selection_drag_start_  = {};
			selection_drag_active_ = false;
			return true;
		}
		return selection_drag_active_;
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
			BeginStroke(ctx, scene, mouse_world);
			FillAt(ctx, scene, mouse_world, image_viewport, presentation_viewport, frame);
			EndStroke(ctx, scene, mouse_world);
			return true;
		}
		return false;
	}

	if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && inside) {
		BeginStroke(ctx, scene, mouse_world);
		if (tool_ == PaintTool::Brush) {
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
