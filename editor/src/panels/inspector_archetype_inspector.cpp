#include "panels/inspector_archetype_inspector.h"

#include <imgui.h>

#include <algorithm>
#include <cfloat>
#include <cstdint>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "editor/editor.h"
#include "editor/paint/paint_editor.h"
#include "runtime/ecs/tag.h"
#include "runtime/world/paint_generator.h"
#include "runtime/world/tilemap.h"

namespace ptgn::editor::inspector {

namespace {

template <typename T, typename Target>
bool DrawAddComponentItem(
	Target& target, const char* label, bool unavailable = false
) {
	if constexpr (!Target::template Supports<T>()) {
		return false;
	} else {
		const bool exists{ target.template Capture<T>().has_value() };
		ScopedDisabled disabled{ exists || unavailable };
		if (!ImGui::MenuItem(label)) {
			return false;
		}
		return SetComponentStateUndoable<Target, T>(
			target,
			std::string{ "Add " } + label,
			ComponentState<T>{ T{} }
		);
	}
}

template <typename Target>
bool DrawAddMovementItem(Target& target) {
	if constexpr (!Target::template Supports<TopDownMovement>() &&
		!Target::template Supports<PlatformerMovement>()) {
		return false;
	} else {
		const bool exists{
			HasInspectorComponent<Target, TopDownMovement>(target) ||
			HasInspectorComponent<Target, PlatformerMovement>(target)
		};
		ScopedDisabled disabled{ exists };
		if (!ImGui::MenuItem("Movement")) {
			return false;
		}

		if constexpr (Target::template Supports<TopDownMovement>()) {
			return SetComponentStateUndoable<Target, TopDownMovement>(
				target,
				"Add Movement",
				ComponentState<TopDownMovement>{ TopDownMovement{} }
			);
		} else {
			return SetComponentStateUndoable<Target, PlatformerMovement>(
				target,
				"Add Movement",
				ComponentState<PlatformerMovement>{ PlatformerMovement{} }
			);
		}
	}
}

template <typename Target>
bool DrawAddComponentMenu(Target& target) {
	bool changed{ false };

	if (ImGui::Button("Add Component", ImVec2{ -FLT_MIN, 0.0f })) {
		ImGui::OpenPopup("AddComponentPopup");
	}

	if (!ImGui::BeginPopup("AddComponentPopup")) {
		return false;
	}

	const bool button_control{ HasInspectorComponent<Target, ::ptgn::impl::ButtonData>(target) };

	changed |= DrawAddComponentItem<Transform>(target, "Transform");
	changed |= DrawAddComponentItem<::ptgn::impl::Interactive>(
		target, "Interaction", button_control
	);

	if (ImGui::BeginMenu("Physics & Movement")) {
		changed |= DrawAddComponentItem<Collider>(target, "Collider");
		changed |= DrawAddComponentItem<RigidBody>(target, "Rigid Body");
		changed |= DrawAddMovementItem(target);
		ImGui::EndMenu();
	}

	changed |= DrawAddComponentItem<::ptgn::impl::Scripts>(target, "Scripts");

	if (ImGui::BeginMenu("Utilities")) {
		changed |= DrawAddComponentItem<::ptgn::impl::Timers>(target, "Timers");
		changed |= DrawAddComponentItem<Group>(target, "Groups");
		changed |= DrawAddComponentItem<Lifetime>(target, "Lifetime");
		ImGui::EndMenu();
	}

	ImGui::EndPopup();
	return changed;
}

[[nodiscard]] const char* GeneratorGeometryName(PaintGeneratorGeometry geometry) {
	switch (geometry) {
		case PaintGeneratorGeometry::Rectangle:   return "Rectangle";
		case PaintGeneratorGeometry::Line:        return "Line";
		case PaintGeneratorGeometry::BrushStroke: return "Brush Stroke";
		case PaintGeneratorGeometry::Infinite:    return "Infinite";
	}
	return "Generator";
}

[[nodiscard]] const char* GeneratorSourceName(PaintGeneratorSourceKind source) {
	switch (source) {
		case PaintGeneratorSourceKind::Single:       return "Single";
		case PaintGeneratorSourceKind::WeightedSet:  return "Weighted Set";
		case PaintGeneratorSourceKind::Checkerboard: return "Checkerboard";
		case PaintGeneratorSourceKind::Autotile:     return "Autotile / Terrain";
		case PaintGeneratorSourceKind::Noise:        return "Noise";
	}
	return "Single";
}

template <typename Target, typename Component, typename Draw>
bool DrawArchetypeComponentSection(
	Target& target,
	std::string_view label,
	std::string_view id,
	Draw&& draw
) {
	const InspectorSectionResult header{
		DrawInspectorSectionHeader(
			label,
			id,
			InspectorSectionOptions{
				.default_open = true,
				.removable = false,
			}
		)
	};

	if (!header.open) {
		return false;
	}

	ScopedIndent indent;
	return DrawRequiredComponent<Target, Component>(
		target,
		label,
		false,
		std::forward<Draw>(draw)
	);
}

template <typename Target>
bool DrawTilemapArchetype(Target& target) {
	return DrawArchetypeComponentSection<Target, ::ptgn::impl::TilemapData>(
		target,
		"Tilemap",
		"TilemapSection",
		[](::ptgn::impl::TilemapData& data) {
			bool changed{ false };

			float cell_size[2]{ data.cell_size.x, data.cell_size.y };
			if (ImGui::DragFloat2("Cell Size", cell_size, 0.25f, 1.0f, 16384.0f, "%.1f")) {
				data.cell_size = {
					std::max(1.0f, cell_size[0]),
					std::max(1.0f, cell_size[1]),
				};
				changed = true;
			}

			int chunk_size[2]{ data.chunk_size.x, data.chunk_size.y };
			if (ImGui::DragInt2("Chunk Size", chunk_size, 0.25f, 1, 4096)) {
				data.chunk_size = {
					std::max(1, chunk_size[0]),
					std::max(1, chunk_size[1]),
				};
				changed = true;
			}

			if (ImGui::TreeNodeEx(
					"Streaming##TilemapStreaming",
					ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
				)) {
				bool streaming_changed{ false };

				streaming_changed |= ImGui::Checkbox("Enabled", &data.streaming.enabled);
				streaming_changed |= ImGui::DragInt(
					"Preload Radius",
					&data.streaming.preload_radius,
					0.15f,
					0,
					1024
				);
				streaming_changed |= ImGui::DragInt(
					"Keep Alive Radius",
					&data.streaming.keep_alive_radius,
					0.15f,
					0,
					1024
				);
				streaming_changed |= ImGui::DragInt(
					"Max Loaded Chunks",
					&data.streaming.max_loaded_chunks,
					0.5f,
					1,
					100000
				);

				if (streaming_changed) {
					data.streaming.preload_radius =
						std::max(0, data.streaming.preload_radius);
					data.streaming.keep_alive_radius = std::max(
						data.streaming.preload_radius,
						data.streaming.keep_alive_radius
					);
					data.streaming.max_loaded_chunks =
						std::max(1, data.streaming.max_loaded_chunks);
					changed = true;
				}

				ImGui::TreePop();
			}

			ImGui::SeparatorText("Authored Data");
			ImGui::TextDisabled("Tiles: %zu", data.tiles.size());
			ImGui::TextDisabled("Terrain cells: %zu", data.terrain.size());
			ImGui::TextDisabled("Excluded cells: %zu", data.exclusion_mask.size());

			ImGui::BeginDisabled(data.exclusion_mask.empty());
			if (ImGui::Button("Clear Exclusion Mask")) {
				data.exclusion_mask.clear();
				changed = true;
			}
			ImGui::EndDisabled();

			return changed;
		}
	);
}


bool DrawGeneratorRecipe(::ptgn::impl::PaintGeneratorData& data) {
	auto& recipe{ data.recipe };
	bool changed{ false };

	if (!ImGui::TreeNodeEx(
			"Recipe##GeneratorRecipe",
			ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
		)) {
		return false;
	}

	int coverage{ static_cast<int>(recipe.coverage) };
	if (ImGui::Combo("Coverage", &coverage, "Solid\0Random Density\0Radial Falloff\0")) {
		recipe.coverage = static_cast<PaintGeneratorCoverageMode>(coverage);
		changed = true;
	}

	if (recipe.coverage == PaintGeneratorCoverageMode::RandomDensity) {
		changed |= ImGui::SliderFloat("Density", &recipe.density, 0.0f, 1.0f, "%.2f");
	} else if (recipe.coverage == PaintGeneratorCoverageMode::RadialFalloff) {
		changed |= ImGui::SliderFloat(
			"Center Density", &recipe.density, 0.0f, 1.0f, "%.2f"
		);
		changed |= ImGui::SliderFloat(
			"Inner Radius", &recipe.radial_inner, 0.0f, 0.95f, "%.2f"
		);
		changed |= ImGui::SliderFloat(
			"Outer Radius", &recipe.radial_outer, 0.05f, 1.0f, "%.2f"
		);
		if (recipe.radial_outer <= recipe.radial_inner) {
			recipe.radial_outer = std::min(1.0f, recipe.radial_inner + 0.01f);
		}
	}

	changed |= ImGui::DragFloat(
		"Minimum Spacing",
		&recipe.min_spacing,
		0.25f,
		0.0f,
		4096.0f,
		"%.1f"
	);
	recipe.min_spacing = std::max(0.0f, recipe.min_spacing);

	changed |= ImGui::Checkbox("Avoid Exclusion Mask", &recipe.avoid_exclusion_mask);
	changed |= ImGui::Checkbox("Linked Prefab Instances", &recipe.link_prefab_instances);

	changed |= ImGui::Checkbox("Random Rotation", &recipe.random_rotation);
	if (recipe.random_rotation) {
		changed |= ImGui::DragFloatRange2(
			"Rotation",
			&recipe.rotation_min,
			&recipe.rotation_max,
			0.5f,
			-3600.0f,
			3600.0f,
			"Min: %.1f",
			"Max: %.1f"
		);
		if (recipe.rotation_min > recipe.rotation_max) {
			std::swap(recipe.rotation_min, recipe.rotation_max);
		}
	}

	changed |= ImGui::Checkbox("Random Scale", &recipe.random_scale);
	if (recipe.random_scale) {
		changed |= ImGui::DragFloatRange2(
			"Scale",
			&recipe.scale_min,
			&recipe.scale_max,
			0.01f,
			0.001f,
			1000.0f,
			"Min: %.2f",
			"Max: %.2f"
		);
		recipe.scale_min = std::max(0.001f, recipe.scale_min);
		recipe.scale_max = std::max(recipe.scale_min, recipe.scale_max);
	}


	ImGui::TreePop();
	return changed;
}

template <typename Target>
bool DrawPaintGeneratorArchetype(Target& target, bool& stop_after_archetype) {
	bool changed{ DrawArchetypeComponentSection<Target, ::ptgn::impl::PaintGeneratorData>(
		target,
		"Paint Generator",
		"PaintGeneratorSection",
		[&target](::ptgn::impl::PaintGeneratorData& data) {
			bool local_changed{ false };

			local_changed |= ImGui::Checkbox("Enabled", &data.enabled);
			ImGui::Text("Geometry: %s", GeneratorGeometryName(data.geometry));

			if constexpr (requires { target.entity; }) {
				if (target.entity && IsPaintGenerator(target.entity)) {
					local_changed |= target.ctx.editor.GetPaintEditor().DrawGeneratorSourceEditor(
						target.ctx,
						target.entity,
						data.recipe
					);
				}
			} else {
				ImGui::Text("Source: %s", GeneratorSourceName(data.recipe.source_kind));
			}

			if (
				data.geometry == PaintGeneratorGeometry::Line ||
				data.geometry == PaintGeneratorGeometry::Rectangle
			) {
				float start[2]{ data.start.x, data.start.y };
				float end[2]{ data.end.x, data.end.y };

				if (ImGui::DragFloat2("Start", start, 0.25f)) {
					data.start = { start[0], start[1] };
					local_changed = true;
				}
				if (ImGui::DragFloat2("End", end, 0.25f)) {
					data.end = { end[0], end[1] };
					local_changed = true;
				}
			}

			if (data.geometry == PaintGeneratorGeometry::Line) {
				local_changed |= ImGui::DragInt(
					"Thickness", &data.line_thickness, 0.15f, 1, 128
				);
				local_changed |= ImGui::DragInt(
					"Spacing", &data.line_spacing, 0.15f, 1, 128
				);
				data.line_thickness = std::clamp(data.line_thickness, 1, 128);
				data.line_spacing = std::clamp(data.line_spacing, 1, 128);
			}

			if (data.geometry == PaintGeneratorGeometry::Rectangle) {
				int area_mode{ static_cast<int>(data.area_mode) };
				if (ImGui::Combo(
						"Area Mode",
						&area_mode,
						"Fill\0Outline\0Corners\0Random Fill\0"
					)) {
					data.area_mode = static_cast<PaintGeneratorAreaMode>(area_mode);
					local_changed = true;
				}

				if (
					data.area_mode == PaintGeneratorAreaMode::Outline ||
					data.area_mode == PaintGeneratorAreaMode::Corners
				) {
					local_changed |= ImGui::DragInt(
						"Area Thickness", &data.area_thickness, 0.15f, 1, 128
					);
					data.area_thickness = std::clamp(data.area_thickness, 1, 128);
				} else if (data.area_mode == PaintGeneratorAreaMode::RandomFill) {
					local_changed |= ImGui::SliderFloat(
						"Random Fill Density",
						&data.random_fill_density,
						0.0f,
						1.0f,
						"%.2f"
					);
				}
			}

			if (data.geometry == PaintGeneratorGeometry::BrushStroke) {
				int shape{ static_cast<int>(data.brush_shape) };
				if (ImGui::Combo("Brush Shape", &shape, "Circle\0Square\0")) {
					data.brush_shape = static_cast<PaintGeneratorBrushShape>(shape);
					local_changed = true;
				}

				struct StrokeRow {
					std::uint32_t id{};
					int diameter{ 1 };
				};

				std::vector<StrokeRow> strokes;
				for (const auto& point : data.stroke_points) {
					const auto it{ std::ranges::find(
						strokes,
						point.stroke_id,
						&StrokeRow::id
					) };
					if (it == strokes.end()) {
						strokes.push_back(StrokeRow{
							.id = point.stroke_id,
							.diameter = std::max(1, point.diameter),
						});
					}
				}
				std::ranges::sort(strokes, {}, &StrokeRow::id);

				if (!strokes.empty() && ImGui::TreeNodeEx(
						"Brush Strokes##GeneratorStrokes",
						ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth
					)) {
					if (ImGui::BeginTable(
							"##GeneratorBrushStrokes",
							2,
							ImGuiTableFlags_SizingStretchProp
						)) {
						ImGui::TableSetupColumn(
							"Stroke", ImGuiTableColumnFlags_WidthStretch
						);
						ImGui::TableSetupColumn(
							"Diameter", ImGuiTableColumnFlags_WidthFixed, 110.0f
						);
						ImGui::TableHeadersRow();

						for (std::size_t index{}; index < strokes.size(); ++index) {
							const StrokeRow row{ strokes[index] };
							ImGui::PushID(static_cast<int>(row.id));
							ImGui::TableNextRow();

							ImGui::TableSetColumnIndex(0);
							ImGui::Text("Stroke %d", static_cast<int>(index + 1));

							ImGui::TableSetColumnIndex(1);
							int diameter{ row.diameter };
							ImGui::SetNextItemWidth(-FLT_MIN);
							if (ImGui::DragInt(
									"##Diameter", &diameter, 0.15f, 1, 128
								)) {
								diameter = std::clamp(diameter, 1, 128);
								for (auto& point : data.stroke_points) {
									if (point.stroke_id == row.id) {
										point.diameter = diameter;
									}
								}
								local_changed = true;
							}
							ImGui::PopID();
						}

						ImGui::EndTable();
					}
					ImGui::TreePop();
				}

				ImGui::TextDisabled("Brush strokes: %zu", strokes.size());
			}

			ImGui::TextDisabled(
				"Suppressed generated cells: %zu",
				data.suppressed_cells.size()
			);
			ImGui::SameLine();
			ImGui::BeginDisabled(data.suppressed_cells.empty());
			if (ImGui::SmallButton("Clear Overrides")) {
				data.suppressed_cells.clear();
				local_changed = true;
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip(
					"Restore manually erased generated output. Regeneration is again allowed "
					"to emit at these cells."
				);
			}

			local_changed |= DrawGeneratorRecipe(data);
			return local_changed;
		}
	) };

	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };
		if (!entity || !IsPaintGenerator(entity)) {
			return changed;
		}

		auto& paint{ target.ctx.editor.GetPaintEditor() };
		Scene& scene{ entity.GetScene() };
		const auto& data{ entity.Get<::ptgn::impl::PaintGeneratorData>() };

		if (paint.IsActiveBrushGenerator(entity)) {
			ImGui::SeparatorText("Live Generator");
			ImGui::TextDisabled(
				"Consecutive Brush strokes are being added to this generator."
			);

			ImGui::BeginDisabled(data.stroke_points.empty());
			if (ImGui::Button("Undo Stroke")) {
				paint.UndoActiveBrushStroke(target.ctx, scene);
				stop_after_archetype = true;
			}
			ImGui::EndDisabled();

			ImGui::SameLine();
			if (ImGui::Button("Finish Generator")) {
				paint.FinishActiveBrushGenerator(target.ctx, scene);
				stop_after_archetype = true;
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel Generator")) {
				paint.CancelActiveBrushGenerator(target.ctx, scene);
				stop_after_archetype = true;
			}
		}

		ImGui::Separator();
		const auto generator_layer_id{ entity.GetScene().GetLayers().GetLayerId(entity) };
		const SceneLayer* generator_layer{
			generator_layer_id
				? entity.GetScene().GetLayers().Find(*generator_layer_id)
				: nullptr
		};
		const bool missing_tilemap{
			generator_layer && generator_layer->kind == SceneLayerKind::Tile &&
			!PaintGenerator{ entity }.GetTargetTilemap()
		};
		ImGui::BeginDisabled(
			data.geometry == PaintGeneratorGeometry::Infinite || missing_tilemap
		);
		if (ImGui::Button("Bake Generator")) {
			paint.BakeGenerator(target.ctx, scene, entity);
			stop_after_archetype = true;
		}
		ImGui::EndDisabled();

		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
			if (data.geometry == PaintGeneratorGeometry::Infinite) {
				ImGui::SetTooltip(
					"Infinite generators cannot be baked without a finite region."
				);
			} else if (missing_tilemap) {
				ImGui::SetTooltip(
					"Tile generators must be children of a Tilemap before they can be baked."
				);
			}
		}
	}

	return changed;
}

template <typename Target>
bool DrawCoreArchetypeSections(
	Target& target,
	InspectorArchetype archetype,
	bool& stop_after_archetype
) {
	bool changed{ false };

	if (GetButtonChildInfo(target)) {
		changed |= DrawUISection(target);
		changed |= DrawTransformSection(target);
		changed |= DrawVisualSection(target);
		return changed;
	}

	if (
		ArchetypeRequiresTransform(archetype) ||
		HasAnyInspectorComponent(target, TransformSectionComponents{})
	) {
		changed |= DrawTransformSection(target);
	}

	// Tilemaps and generators are first-class archetypes. Their identity-owning components are
	// edited here rather than being routed to a separate PaintEditor inspector.
	if (archetype == InspectorArchetype::Tilemap) {
		changed |= DrawTilemapArchetype(target);
	} else if (archetype == InspectorArchetype::PaintGenerator) {
		changed |= DrawPaintGeneratorArchetype(target, stop_after_archetype);
	}

	if (stop_after_archetype) {
		return changed;
	}

	if (ArchetypeOwnsUI(archetype) || HasAnyInspectorComponent(target, UISectionComponents{})) {
		changed |= DrawUISection(target);
	}

	// RenderTarget is explicitly archetype-routed and owns a "Render Target" section header.
	// Other visual archetypes use their own archetype label. Generic entities retain "Visual".
	if (archetype == InspectorArchetype::RenderTarget) {
		changed |= DrawVisualSection(target, true, false, "Render Target");
	} else if (ArchetypeOwnsVisual(archetype)) {
		changed |= DrawVisualSection(
			target,
			true,
			false,
			GetInspectorArchetypeLabel(target)
		);
	} else if (HasVisualSection(target)) {
		changed |= DrawVisualSection(target, true, true, "Visual");
	}

	// Camera is likewise explicitly archetype-routed. DrawCameraSection owns the "Camera" header.
	if (archetype == InspectorArchetype::Camera) {
		changed |= DrawCameraSection(target);
	} else if (HasCameraSection(target)) {
		changed |= DrawCameraSection(target);
	}

	return changed;
}

template <typename Target>
bool DrawArchetypeInspectorImpl(Target& target) {
	if constexpr (requires { target.entity; }) {
		ClearButtonPreviewIfDifferent(target.ctx, target.entity);
	} else {
		ClearButtonPreviewIfDifferent(target.ctx);
	}

	const InspectorArchetype archetype{ ResolveInspectorArchetype(target) };
	bool stop_after_archetype{ false };
	bool changed{
		DrawCoreArchetypeSections(target, archetype, stop_after_archetype)
	};

	if (stop_after_archetype) {
		return changed;
	}

	changed |= DrawInteractionSection(target);
	changed |= DrawPhysicsSection(target);
	changed |= DrawScriptsSection(target);
	changed |= DrawUtilitiesSection(target);

	ImGui::Separator();
	changed |= DrawAddComponentMenu(target);
	return changed;
}

} // namespace

bool DrawArchetypeInspector(EntityInspectorTarget& target) {
	return DrawArchetypeInspectorImpl(target);
}

bool DrawArchetypeInspector(PrefabInspectorTarget& target) {
	return DrawArchetypeInspectorImpl(target);
}

} // namespace ptgn::editor::inspector
