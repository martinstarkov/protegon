#include "panels/inspector_internal.h"

#include <imgui.h>
#include <imgui_stdlib.h>
#include <magic_enum/magic_enum.hpp>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "editor/editor.h"
#include "editor/editor_context.h"
#include "core/graphics/color.h"
#include "core/graphics/fill_style.h"
#include "core/math/angle.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/origin.h"
#include "core/math/geometry/rect.h"
#include "core/math/matrix4.h"
#include "core/math/vector2.h"
#include "core/math/vector3.h"
#include "core/math/vector4.h"
#include "panels/inspector_feature_helpers.h"
#include "panels/inspector_fields.h"
#include "panels/scene_hierarchy.h"
#include "renderer/pipeline/blend_mode.h"
#include "renderer/pipeline/render_state.h"
#include "renderer/renderer.h"
#include "renderer/text/font_style.h"
#include "renderer/text/text_layout.h"
#include "renderer/text/text_style.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/asset/asset_key.h"
#include "runtime/ecs/component_registry.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/drawable.h"
#include "runtime/graphics/fx/bloom.h"
#include "runtime/graphics/fx/blur.h"
#include "runtime/graphics/fx/gaussian_blur.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/fx/particle.h"
#include "runtime/graphics/custom_shader.h"
#include "runtime/graphics/graphics.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scripting/script.h"
#include "runtime/ui/button.h"
#include "runtime/ui/button_config.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"

namespace magic_enum::customize {

template <>
struct enum_range<ptgn::TextureFormat> {
	static constexpr int min{ static_cast<int>(ptgn::TextureFormat::RGB8) };
	static constexpr int max{ static_cast<int>(ptgn::TextureFormat::Stencil8) };
};

} // namespace magic_enum::customize

namespace ptgn::editor::inspector {

template <typename T>
struct ComponentDrawer {
	static bool Draw(EditorContext& ctx, T& value)
		requires (!std::is_empty_v<T> && kHasDefaultInspectorDrawer<T>)
	{
		return DrawDefaultContents(ctx, value);
	}
};

template <typename T>
inline constexpr bool kHasComponentDrawer{
	requires(EditorContext& ctx, T& value) {
		ComponentDrawer<T>::Draw(ctx, value);
	}
};

template <>
struct ComponentDrawer<::ptgn::impl::Scripts> {
	static bool Draw(EditorContext& ctx, ::ptgn::impl::Scripts& scripts) {
		return DrawScriptsComponent(ctx, scripts);
	}
};

bool DrawOptionalViewport(
	EditorContext& ctx, std::string_view label, std::optional<Viewport>& value,
	ViewportSpace viewport_space
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	bool changed{
		DrawOptionalLabelRow(
			label,
			enabled,
			IsReadOnly()
		)
	};

	if (enabled != value.has_value()) {
		if (enabled) {
			auto& viewport{ value.emplace() };

			if (viewport_space == ViewportSpace::Normalized) {
				viewport.position = V2_float{ 0.0f, 0.0f };
				viewport.size	  = V2_float{ 1.0f, 1.0f };
			}
		} else {
			value.reset();
		}

		changed = true;
	}

	if (value.has_value()) {
		ScopedIndent contents_indent;
		ScopedPropertyLabelOffset label_offset{
			ImGui::GetStyle().IndentSpacing
		};
		if (viewport_space == ViewportSpace::Normalized) {
			FieldOptions options{
				.speed	= 0.01f,
				.min	= 0.0,
				.max	= 1.0,
				.format = "%.3f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			};

			changed |= DrawValue(ctx, "Position", value->position, options);
			changed |= DrawWHValue(
				"Size", value->size, options.speed, static_cast<float>(options.min),
				static_cast<float>(options.max), options.format, options.flags
			);

			if (value->size.x > 1.0f || value->size.y > 1.0f) {
				value->size.x = std::min(1.0f, value->size.x);
				value->size.y = std::min(1.0f, value->size.y);
				changed		  = true;
			}
		} else {
			changed |= DrawValue(
				ctx, "Position", value->position,
				FieldOptions{
					.speed	= 1.0f,
					.format = "%.0f",
				}
			);

			changed |= DrawWHValue(
				"Size", value->size, 1.0f, 1.0f, 4096.0f, "%.0f",
				ImGuiSliderFlags_AlwaysClamp
			);

			if (value->size.x < 1.0f || value->size.y < 1.0f) {
				value->size.x = std::max(1.0f, value->size.x);
				value->size.y = std::max(1.0f, value->size.y);
				changed		  = true;
			}
		}

	}

	ImGui::PopID();

	return changed;
}

void MarkTextLayoutDirty(Entity entity) {
	if (entity.Has<TextLayout>()) {
		entity.Get<TextLayout>().dirty = true;
	}
}

void MarkButtonDirty(Entity entity, ::ptgn::impl::ButtonDirty dirty) {
	Entity button{ entity };

	if (!button.Has<::ptgn::impl::ButtonData>()) {
		button = GetParent(entity);
	}

	if (!button || !button.Has<::ptgn::impl::ButtonData>()) {
		return;
	}

	button.Get<::ptgn::impl::ButtonData>().dirty |= dirty;
}

void MarkButtonTextDirty(Entity entity) {
	MarkTextLayoutDirty(entity);
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Text);
}

void MarkButtonBorderDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Border);
}

void MarkButtonBackgroundDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Background);
}

void MarkButtonSpriteDirty(Entity entity) {
	MarkButtonDirty(entity, ::ptgn::impl::ButtonDirty::Sprite);
}


bool DrawTextureFormatCombo(
	const char* label,
	TextureFormat& format,
	bool color_only = true
) {
	ImGui::PushID(&format);

	const bool changed{
		DrawPropertyRow(label, [&]() {
			const std::string_view preview{
				magic_enum::enum_name(format)
			};

			bool local_changed{ false };

			ImGui::SetNextItemWidth(-FLT_MIN);

			if (ImGui::BeginCombo(
					"##TextureFormat",
					preview.empty()
						? ToString(format).data()
						: preview.data()
				)) {
				for (
					const TextureFormat candidate :
					magic_enum::enum_values<TextureFormat>()
				) {
					if (
						color_only &&
						!IsColorFormat(candidate)
					) {
						continue;
					}

					const std::string_view name{
						magic_enum::enum_name(candidate)
					};

					const bool selected{
						candidate == format
					};

					if (ImGui::Selectable(
							name.data(),
							selected
						)) {
						format = candidate;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		})
	};

	ImGui::PopID();

	return changed;
}

bool DrawRenderTargetDesc(
	EditorContext& ctx,
	::ptgn::impl::RenderTargetDesc& target,
	std::optional<V2_int> framebuffer_size,
	bool size_read_only
) {
	V2_int followed_size{
		framebuffer_size.value_or(V2_int{})
	};

	if (!followed_size.IsPositive()) {
		auto& renderer{
			ctx.editor.GetRenderer()
		};

		followed_size = renderer.GetDisplaySize();

		if (!followed_size.IsPositive()) {
			followed_size =
				renderer.GetPresentationSize();
		}
	}

	const bool was_following{
		target.follow_display_size
	};

	bool changed{
		DrawValue(
			ctx,
			"Follow Display Size",
			target.follow_display_size,
			FieldOptions{
				.read_only = size_read_only,
			}
		)
	};

	if (
		!size_read_only &&
		was_following &&
		!target.follow_display_size &&
		followed_size.IsPositive()
	) {
		target.size = followed_size;
	}

	V2_int displayed_size{
		target.follow_display_size &&
			followed_size.IsPositive()
			? followed_size
			: target.size
	};

	const bool size_changed{
		DrawWHValue(
			"Framebuffer Size",
			displayed_size,
			1.0f,
			1,
			4096,
			ImGuiSliderFlags_AlwaysClamp,
			size_read_only ||
				target.follow_display_size
		)
	};

	if (
		size_changed &&
		!size_read_only &&
		!target.follow_display_size
	) {
		target.size = displayed_size;
		changed = true;
	}

	// Intentionally editable for the scene target.
	changed |= DrawTextureFormatCombo(
		"Framebuffer Format",
		target.format
	);

	return changed;
}

template <>
struct ComponentDrawer<::ptgn::impl::RenderTargetDesc> {
	static bool Draw(
		EditorContext& ctx,
		::ptgn::impl::RenderTargetDesc& target
	) {
		return DrawRenderTargetDesc(
			ctx,
			target
		);
	}
};

bool DrawOptionalBoundingBox(
	EditorContext& ctx,
	std::string_view label,
	std::optional<BoundingBox>& value
) {
	ImGui::PushID(&value);

	bool enabled{ value.has_value() };
	const bool read_only{ IsReadOnly() };
	bool changed{
		DrawOptionalLabelRow(
			label,
			enabled,
			read_only
		)
	};

	if (enabled != value.has_value()) {
		if (enabled) {
			value.emplace();
		} else {
			value.reset();
		}

		changed = true;
	}

	if (value.has_value()) {
		ScopedIndent contents_indent;
		ScopedPropertyLabelOffset label_offset{
			ImGui::GetStyle().IndentSpacing
		};
		ScopedDisabled disabled{ read_only };

		BoundingBox displayed{ *value };
		bool contents_changed{ false };

		contents_changed |= DrawValue(
			ctx,
			"Position",
			displayed.position,
			FieldOptions{
				.speed = kInspectorPositionDragSpeed,
				.format = "%.3f",
			}
		);

		auto& rect{ displayed.rect };
		V2_float size{ rect.GetSize() };

		if (DrawWHValue(
				"Size",
				size,
				kInspectorSizeDragSpeed,
				0.0f,
				0.0f,
				"%.3f"
			)) {
			size.x = std::max(size.x, 0.0f);
			size.y = std::max(size.y, 0.0f);

			const V2_float center{ rect.GetCenter() };
			const V2_float half_size{ size * 0.5f };

			rect.min = center - half_size;
			rect.max = center + half_size;
			contents_changed = true;
		}

		contents_changed |= DrawValue(
			ctx,
			"Min",
			rect.min,
			FieldOptions{
				.speed = kInspectorPositionDragSpeed,
				.format = "%.3f",
			}
		);
		contents_changed |= DrawValue(
			ctx,
			"Max",
			rect.max,
			FieldOptions{
				.speed = kInspectorPositionDragSpeed,
				.format = "%.3f",
			}
		);
		contents_changed |= DrawValue(
			ctx,
			"Origin",
			displayed.origin
		);

		if (!read_only && contents_changed) {
			value = displayed;
			changed = true;
		}
	}

	ImGui::PopID();
	return changed;
}

template <>
struct ComponentDrawer<::ptgn::impl::CameraData> {
	static bool Draw(EditorContext& ctx, ::ptgn::impl::CameraData& camera) {
		bool changed{ false };

		changed |= DrawValue(ctx, "Viewport Space", camera.viewport_space);
		changed |= DrawOptionalViewport(
			ctx,
			"Raw Viewport",
			camera.raw_viewport,
			camera.viewport_space
		);
		changed |= DrawOptionalBoundingBox(
			ctx,
			"Bounding Box",
			camera.bounding_box
		);
		changed |= DrawValue(ctx, "Pixel Rounding", camera.pixel_rounding);

		if (ctx.local.settings.show_read_only_inspector_data) {
			DrawReadOnlyValue(ctx, "View Projection", camera.view_projection);
		}

		return changed;
	}
};

namespace {

bool DrawLayerMaskValueImpl(
	std::string_view label,
	LayerMask& value,
	bool* ui_layer
) {
	ScopedID scope{ label };
	const char* preview{ nullptr };
	char raw_preview[32]{};

	if (ui_layer && *ui_layer) {
		preview = "UI Layer";
	} else if (value == kLayersAll) {
		preview = "All";
	} else if (value == kLayersNone) {
		preview = "None";
	} else if (value == kLayerDefault) {
		preview = "Default";
	} else {
		std::snprintf(
			raw_preview,
			sizeof(raw_preview),
			"0x%016llX",
			static_cast<unsigned long long>(value)
		);
		preview = raw_preview;
	}

	return DrawPropertyRow(
		label,
		[&]() {
			bool changed{ false };
			ImGui::SetNextItemWidth(-FLT_MIN);

			auto choose_mask = [&](LayerMask mask) {
				value = mask;
				if (ui_layer) {
					*ui_layer = false;
				}
				changed = true;
			};

			if (ImGui::BeginCombo("##LayerMask", preview)) {
				const bool regular_layers_active{
					!ui_layer || !*ui_layer
				};

				if (ui_layer) {
					bool selected{ *ui_layer };

					if (ImGui::Checkbox("UI Layer", &selected)) {
						*ui_layer = selected;
						value = selected
							? kLayersNone
							: ::ptgn::impl::RenderMask{}.layers;
						changed = true;
					}

					DrawTooltip(
						"UI Layer adds the UI layer tag. Selecting a normal render layer removes it."
					);
					ImGui::Separator();
				}

				if (ImGui::Selectable("All", regular_layers_active && value == kLayersAll)) {
					choose_mask(kLayersAll);
				}

				if (ImGui::Selectable("None", regular_layers_active && value == kLayersNone)) {
					choose_mask(kLayersNone);
				}

				if (ImGui::Selectable("Default", regular_layers_active && value == kLayerDefault)) {
					choose_mask(kLayerDefault);
				}

				ImGui::Separator();

				if (ImGui::BeginChild(
						"##LayerMaskValues",
						ImVec2{ 0.0f, ImGui::GetTextLineHeightWithSpacing() * 10.0f },
						ImGuiChildFlags_Borders
					)) {
					for (int index{ 0 }; index < 64; ++index) {
						const LayerMask layer{ GetLayer(index) };
						bool selected{
							regular_layers_active &&
							(value & layer) != 0
						};
						const std::string layer_label{
							index == 0
								? "Layer 0 (Default)"
								: std::string{ "Layer " } + std::to_string(index)
						};

						ImGui::PushID(index);

						if (ImGui::Checkbox(layer_label.c_str(), &selected)) {
							if (ui_layer) {
								*ui_layer = false;
							}

							if (selected) {
								value |= layer;
							} else {
								value &= ~layer;
							}

							changed = true;
						}

						ImGui::PopID();
					}
				}

				ImGui::EndChild();
				ImGui::EndCombo();
			}

			return changed;
		}
	);
}

} // namespace

bool DrawLayerMaskValue(
	std::string_view label,
	LayerMask& value
) {
	return DrawLayerMaskValueImpl(label, value, nullptr);
}

bool DrawLayerMaskValue(
	std::string_view label,
	LayerMask& value,
	bool& ui_layer
) {
	return DrawLayerMaskValueImpl(label, value, &ui_layer);
}

template <>
struct ComponentDrawer<::ptgn::impl::RenderMask> {
	static bool Draw(EditorContext&, ::ptgn::impl::RenderMask& mask) {
		return DrawLayerMaskValue(
			"Layers",
			mask.layers
		);
	}
};

template <>
struct ComponentDrawer<::ptgn::impl::CameraMask> {
	static bool Draw(EditorContext&, ::ptgn::impl::CameraMask& mask) {
		bool changed{ false };
		changed |= DrawLayerMaskValue(
			"Include Layer",
			mask.include
		);
		changed |= DrawLayerMaskValue(
			"Exclude Layer",
			mask.exclude
		);
		return changed;
	}
};

template <>
struct ComponentDrawer<::ptgn::impl::IDrawable> {
	static bool Draw(EditorContext&, ::ptgn::impl::IDrawable& drawable) {
		auto* current_info{ ::ptgn::impl::IDrawable::FindInfo(drawable.hash) };

		std::string preview{ current_info ? std::string{ current_info->GetDisplayName() }
										  : "<Unregistered Drawable>" };

		bool changed{ DrawPropertyRow("Drawable", [&]() {
			bool local_changed{ false };

			if (ImGui::BeginCombo("##value", preview.c_str())) {
				for (const auto& info : ::ptgn::impl::IDrawable::data()) {
					bool selected{ drawable.hash == info.hash };
					std::string display_name{ info.GetDisplayName() };

					if (ImGui::Selectable(display_name.c_str(), selected)) {
						drawable.hash = info.hash;
						local_changed = true;
					}

					if (selected) {
						ImGui::SetItemDefaultFocus();
					}
				}

				ImGui::EndCombo();
			}

			return local_changed;
		}) };

		if (!current_info && drawable.hash != 0) {
			ImGui::TextDisabled("Stored hash: %zu", drawable.hash);
		}

		return changed;
	}
};

// Only types whose default reflected layout is not ideal need a specialization.
template <>
struct ComponentDrawer<Hollow> {
	static bool Draw(EditorContext& ctx, Hollow& hollow) {
		return DrawValue(
			ctx, "Line Width", hollow.line_width,
			FieldOptions{
				.speed	= 0.1f,
				.min	= kInspectorMinLineWidth,
				.max	= 1000.0f,
				.format = "%.2f",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}
};

template <>
struct ComponentDrawer<FillStyle> {
	static bool Draw(EditorContext& ctx, FillStyle& style) {
		return DrawFillStyle(
			ctx,
			"Style",
			style
		);
	}
};

[[nodiscard]] PositionPicker::Convert MakeInspectorPositionConverter(
	EditorContext& ctx,
	std::optional<Transform> local_transform = std::nullopt
) {
	Entity entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };

	if (entity) {
		return [
			entity,
			local_transform
		](V2_float world_position) mutable -> std::optional<V2_float> {
			if (!entity) {
				return std::nullopt;
			}

			V2_float local_position{
				GetDrawTransform(entity).ApplyInverse(world_position)
			};

			if (local_transform) {
				local_position = local_transform->ApplyInverse(local_position);
			}

			return local_position;
		};
	}

	if (local_transform) {
		return [
			local_transform
		](V2_float world_position) -> std::optional<V2_float> {
			return local_transform->ApplyInverse(world_position);
		};
	}

	return {};
}

template <typename Component, typename Accessor>
[[nodiscard]] PositionPicker::Apply MakeComponentPositionApply(
	EditorContext& ctx,
	Accessor accessor
) {
	Entity entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	Editor* editor{ &ctx.editor };

	return [
		entity,
		editor,
		accessor = std::move(accessor)
	](V2_float picked) mutable {
		if (!entity || !entity.Has<Component>()) {
			return;
		}

		auto& component{ entity.Get<Component>() };
		accessor(component) = picked;
		editor->MarkProjectDirty();
	};
}

template <typename ShapeType, typename Accessor>
[[nodiscard]] PositionPicker::Apply MakeGraphicsPositionApply(
	EditorContext& ctx,
	std::size_t command_index,
	Accessor accessor
) {
	Entity entity{ ctx.editor.GetSceneHierarchyPanel().GetSelectedEntity() };
	Editor* editor{ &ctx.editor };

	return [
		entity,
		editor,
		command_index,
		accessor = std::move(accessor)
	](V2_float picked) mutable {
		if (!entity || !entity.Has<::ptgn::impl::GraphicsData>()) {
			return;
		}

		auto& commands{
			entity.Get<::ptgn::impl::GraphicsData>().commands_
		};
		if (command_index >= commands.size()) {
			return;
		}

		auto member{ ReflectValue(commands[command_index].shape) };
		auto* shape{ std::get_if<ShapeType>(&member.value) };
		if (!shape) {
			return;
		}

		accessor(*shape) = picked;
		editor->MarkProjectDirty();
	};
}

using IndexedPositionApply =
	std::function<PositionPicker::Apply(std::size_t)>;

bool DrawPickablePosition(
	EditorContext& ctx,
	std::string_view label,
	V2_float& position,
	const PositionPicker::Convert& convert = {},
	PositionPicker::Apply apply = {},
	bool* remove_requested = nullptr
) {
	return DrawPropertyRow(
		label,
		[&]() {
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const float pick_width{
				ImGui::CalcTextSize("Pick").x +
				ImGui::GetStyle().FramePadding.x * 2.0f
			};
			const float remove_width{
				remove_requested ? ImGui::GetFrameHeight() : 0.0f
			};
			const float remove_spacing{
				remove_requested ? spacing : 0.0f
			};
			const float available{ ImGui::GetContentRegionAvail().x };
			const float field_width{
				std::max(
					36.0f,
					(
						available -
						pick_width -
						remove_width -
						remove_spacing -
						spacing * 2.0f
					) * 0.5f
				)
			};

			bool changed{ false };

			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##X",
				&position.x,
				kInspectorScalarDragSpeed,
				0.0f,
				0.0f,
				"X %.2f"
			);

			ImGui::SameLine(0.0f, spacing);
			ImGui::SetNextItemWidth(field_width);
			changed |= ImGui::DragFloat(
				"##Y",
				&position.y,
				kInspectorScalarDragSpeed,
				0.0f,
				0.0f,
				"Y %.2f"
			);

			ImGui::SameLine(0.0f, spacing);

			if (!apply) {
				V2_float* target{ &position };
				Editor* editor{ &ctx.editor };

				apply = PositionPicker::Apply{
					[target, editor](V2_float picked) {
						*target = picked;
						editor->MarkProjectDirty();
					}
				};
			}

			(void)DrawPositionPickButton(
				ctx,
				label,
				position,
				PositionPicker::Convert{ convert },
				apply
			);

			if (remove_requested) {
				ImGui::SameLine(0.0f, spacing);
				if (ImGui::Button(
						"-",
						ImVec2{
							ImGui::GetFrameHeight(),
							ImGui::GetFrameHeight()
						}
					)) {
					*remove_requested = true;
				}
				DrawTooltip("Delete this vertex.");
			}

			return changed;
		}
	);
}

bool DrawRectContents(
	EditorContext& ctx,
	Rect& rect,
	const PositionPicker::Convert& convert,
	PositionPicker::Apply min_apply = {},
	PositionPicker::Apply max_apply = {}
) {
	bool changed{ false };

	auto size{ rect.max - rect.min };

	if (DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
		size.x = std::max(size.x, 0.0f);
		size.y = std::max(size.y, 0.0f);

		auto center{ rect.GetCenter() };
		auto half_size{ size * 0.5f };

		rect.min = center - half_size;
		rect.max = center + half_size;

		changed = true;
	}

	changed |= DrawPickablePosition(
		ctx,
		"Min",
		rect.min,
		convert,
		std::move(min_apply)
	);
	changed |= DrawPickablePosition(
		ctx,
		"Max",
		rect.max,
		convert,
		std::move(max_apply)
	);

	return changed;
}

template <>
struct ComponentDrawer<Rect> {
	static bool Draw(EditorContext& ctx, Rect& rect) {
		return DrawRectContents(
			ctx,
			rect,
			MakeInspectorPositionConverter(ctx),
			MakeComponentPositionApply<Rect>(
				ctx,
				[](Rect& value) -> V2_float& {
					return value.min;
				}
			),
			MakeComponentPositionApply<Rect>(
				ctx,
				[](Rect& value) -> V2_float& {
					return value.max;
				}
			)
		);
	}
};

template <>
struct ComponentDrawer<TextRun> {
	static bool Draw(EditorContext& ctx, TextRun& run) {
		bool changed{ false };
		changed |= DrawValue(ctx, "Text", run.text, 
		FieldOptions{
			.multiline	   = true,
			.line_count	   = 8,
			.resizable_y   = true,
			.large_editor  = true,
		});
		changed |= DrawValue(ctx, "Font", run.font);
		changed |= DrawValue(ctx, "Color", run.style.color);
		changed |= DrawValue(ctx, "Size", run.style.size);

		const bool style_open{
			ImGui::TreeNodeEx(
				"Style##TextRunStyle",
				ImGuiTreeNodeFlags_SpanAvailWidth
			)
		};

		if (style_open) {
			{
				ScopedUnindent align_with_style;
				changed |= DrawValue(ctx, "Bold Weight", run.style.bold_weight);
				changed |= DrawValue(ctx, "Kerning", run.style.kerning);
				changed |= DrawValue(ctx, "Tracking", run.style.tracking);
				changed |= DrawValue(ctx, "Line Spacing", run.style.line_spacing);
				changed |= DrawValue(ctx, "Flags", run.style.flags);
				changed |= DrawValue(ctx, "Distance Field", run.style.sdf);
				changed |= DrawValue(ctx, "Effect", run.style.effect);
			}

			ImGui::TreePop();
		}

		return changed;
	}
};

template <>
struct ComponentDrawer<StyledText> {
	static bool Draw(EditorContext& ctx, StyledText& text) {
		TextRunDefaults defaults{};
		if (!text.runs.empty()) {
			defaults.font = text.runs.front().font;
			defaults.style = text.runs.front().style;
		}

		std::string source{ SerializeStyledTextToRichText(text, defaults) };

		if (!DrawRichTextEditor(ctx, source, defaults)) {
			return false;
		}

		text = ParseRichText(source, defaults).text;
		return true;
	}
};

template <>
struct ComponentDrawer<::ptgn::impl::TextData> {
	static bool Draw(EditorContext& ctx, ::ptgn::impl::TextData& data) {
		bool changed{ false };

		TextRunDefaults defaults{};
		if (!data.text.runs.empty()) {
			defaults.font = data.text.runs.front().font;
			defaults.style = data.text.runs.front().style;
		}

		std::string source{ SerializeStyledTextToRichText(data.text, defaults) };
		if (DrawRichTextEditor(ctx, source, defaults)) {
			data.text = ParseRichText(source, defaults).text;
			data.current_run_index = data.text.runs.empty() ? 0 : data.text.runs.size() - 1;
			changed = true;
		}

		changed |= DrawValue(ctx, "Text Box", data.box);
		changed |= DrawValue(ctx, "Reveal Glyph Count", data.glyph_count);
		changed |= DrawValue(ctx, "Clip", data.clip);

		return changed;
	}
};

template <typename T>
[[nodiscard]] std::string GraphicsShapeTypeLabel() {
	using Value = std::remove_cvref_t<T>;
	if constexpr (std::same_as<Value, V2_float>) {
		return "Point";
	} else {
		return VariantTypeLabel<Value>();
	}
}

bool DrawRadiusValue(
	EditorContext& ctx,
	float& radius
) {
	return DrawValue(
		ctx,
		"Radius",
		radius,
		FieldOptions{
			.speed = kInspectorScalarDragSpeed,
			.format = "R: %.3f",
		}
	);
}

bool DrawPolygonContents(
	EditorContext& ctx,
	Polygon& polygon,
	const PositionPicker::Convert& convert,
	const IndexedPositionApply& apply_for_vertex = {}
) {
	bool changed{ false };

	{
		ScopedDisabled disabled{ IsPositionPickingActive(ctx) };

		if (ImGui::Button(
				"+ Vertex",
				ImVec2{ -FLT_MIN, 0.0f }
			)) {
			polygon.vertices.emplace_back();
			changed = true;
		}
	}

	std::optional<std::size_t> remove;

	for (std::size_t index{ 0 }; index < polygon.vertices.size(); ++index) {
		ScopedID vertex_scope{ static_cast<int>(index) };
		bool remove_vertex{ false };

		changed |= DrawPickablePosition(
			ctx,
			"Vertex " + std::to_string(index + 1),
			polygon.vertices[index],
			convert,
			apply_for_vertex
				? apply_for_vertex(index)
				: PositionPicker::Apply{},
			&remove_vertex
		);

		if (remove_vertex) {
			remove = index;
		}
	}

	if (remove) {
		polygon.vertices.erase(
			polygon.vertices.begin() +
			static_cast<std::ptrdiff_t>(*remove)
		);
		changed = true;
	}

	return changed;
}

template <typename ShapeType>
bool DrawGraphicsLine(
	EditorContext& ctx,
	Line& line,
	const PositionPicker::Convert& convert,
	std::size_t command_index,
	Line ShapeType::* line_member = nullptr
) {
	auto make_apply = [&](bool start) {
		if constexpr (std::same_as<ShapeType, Line>) {
			return MakeGraphicsPositionApply<Line>(
				ctx,
				command_index,
				[start](Line& value) -> V2_float& {
					return start ? value.start : value.end;
				}
			);
		} else {
			return MakeGraphicsPositionApply<ShapeType>(
				ctx,
				command_index,
				[line_member, start](ShapeType& value) -> V2_float& {
					auto& member{ value.*line_member };
					return start ? member.start : member.end;
				}
			);
		}
	};

	bool changed{ false };
	changed |= DrawPickablePosition(
		ctx,
		"Start",
		line.start,
		convert,
		make_apply(true)
	);
	changed |= DrawPickablePosition(
		ctx,
		"End",
		line.end,
		convert,
		make_apply(false)
	);
	return changed;
}

bool DrawGraphicsRoundedRect(
	EditorContext& ctx,
	RoundedRect& rounded_rect,
	const PositionPicker::Convert& convert,
	std::size_t command_index
) {
	bool changed{
		DrawRectContents(
			ctx,
			rounded_rect.rect,
			convert,
			MakeGraphicsPositionApply<RoundedRect>(
				ctx,
				command_index,
				[](RoundedRect& value) -> V2_float& {
					return value.rect.min;
				}
			),
			MakeGraphicsPositionApply<RoundedRect>(
				ctx,
				command_index,
				[](RoundedRect& value) -> V2_float& {
					return value.rect.max;
				}
			)
		)
	};

	if constexpr (ReflectedMembers<RoundedRect>) {
		auto members{ ReflectMembers(rounded_rect) };

		auto draw_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;

			if constexpr (!std::same_as<Member, Rect>) {
				ImGui::PushID(
					member.name.data(),
					member.name.data() + member.name.size()
				);

				if constexpr (std::same_as<Member, float>) {
					if (member.name == "radius") {
						changed |= DrawRadiusValue(
							ctx,
							member.value
						);
					} else {
						changed |= DrawValue(
							ctx,
							PrettyName(member.name),
							member.value
						);
					}
				} else {
					changed |= DrawValue(
						ctx,
						PrettyName(member.name),
						member.value
					);
				}

				ImGui::PopID();
			}
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);
	}

	return changed;
}

bool DrawGraphicsCapsule(
	EditorContext& ctx,
	Capsule& capsule,
	const PositionPicker::Convert& convert,
	std::size_t command_index
) {
	bool changed{ false };
	changed |= DrawGraphicsLine<Capsule>(
		ctx,
		capsule.line,
		convert,
		command_index,
		&Capsule::line
	);
	changed |= DrawRadiusValue(ctx, capsule.radius);
	return changed;
}

bool DrawGraphicsTriangle(
	EditorContext& ctx,
	Triangle& triangle,
	const PositionPicker::Convert& convert,
	std::size_t command_index
) {
	bool changed{ false };

	for (std::size_t i{ 0 }; i < triangle.vertices.size(); ++i) {
		changed |= DrawPickablePosition(
			ctx,
			"Vertex " + std::to_string(i + 1),
			triangle.vertices[i],
			convert,
			MakeGraphicsPositionApply<Triangle>(
				ctx,
				command_index,
				[i](Triangle& value) -> V2_float& {
					PTGN_ASSERT(i < value.vertices.size());
					return value.vertices[i];
				}
			)
		);
	}

	return changed;
}

bool DrawGraphicsArc(
	EditorContext& ctx,
	Arc& arc
) {
	bool changed{ DrawRadiusValue(ctx, arc.radius) };

	if constexpr (ReflectedMembers<Arc>) {
		auto members{ ReflectMembers(arc) };

		auto draw_member = [&](auto&& member) {
			if (member.name == "radius") {
				return;
			}

			ImGui::PushID(
				member.name.data(),
				member.name.data() + member.name.size()
			);
			changed |= DrawValue(
				ctx,
				PrettyName(member.name),
				member.value
			);
			ImGui::PopID();
		};

		std::apply(
			[&](auto&&... member) {
				(draw_member(member), ...);
			},
			members
		);
	}

	return changed;
}

bool ForceGraphicsLineStyleHollow(
	::ptgn::impl::GraphicsCommand& command
) {
	if (
		!command.shape.HoldsAlternative<Line>() ||
		command.line_width.GetLineWidth().has_value()
	) {
		return false;
	}

	command.line_width = FillStyle{ 1.0f };
	return true;
}

template <typename... T>
bool DrawGraphicsShape(
	EditorContext& ctx,
	std::variant<T...>& shape,
	const PositionPicker::Convert& convert,
	std::size_t command_index
) {
	static const auto names{ std::array<std::string, sizeof...(T)>{
		GraphicsShapeTypeLabel<T>()...
	} };

	ScopedID shape_scope{ "GraphicsShape" };
	std::optional<std::size_t> requested_index;

	bool changed{ DrawPropertyRow("Shape", [&]() {
		const std::size_t index{ shape.index() };
		if (ImGui::BeginCombo("##Type", names[index].c_str())) {
			for (std::size_t i{ 0 }; i < names.size(); ++i) {
				const bool selected{ i == index };
				if (ImGui::Selectable(names[i].c_str(), selected) && !selected) {
					requested_index = i;
				}
				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}
		return requested_index.has_value();
	}) };

	if (requested_index) {
		EmplaceVariant(shape, *requested_index);
		return true;
	}

	std::visit(
		[&]<typename TValue>(TValue& active) {
			using Value = std::remove_cvref_t<TValue>;
			if constexpr (!std::is_empty_v<Value> || !kHasNoReflectedContents<Value>) {
				ScopedIndent indent;
				if constexpr (std::same_as<Value, V2_float>) {
					changed |= DrawPickablePosition(
						ctx,
						"Point",
						active,
						convert,
						MakeGraphicsPositionApply<V2_float>(
							ctx,
							command_index,
							[](V2_float& value) -> V2_float& {
								return value;
							}
						)
					);
				} else if constexpr (std::same_as<Value, Rect>) {
					changed |= DrawRectContents(
						ctx,
						active,
						convert,
						MakeGraphicsPositionApply<Rect>(
							ctx,
							command_index,
							[](Rect& value) -> V2_float& {
								return value.min;
							}
						),
						MakeGraphicsPositionApply<Rect>(
							ctx,
							command_index,
							[](Rect& value) -> V2_float& {
								return value.max;
							}
						)
					);
				} else if constexpr (std::same_as<Value, RoundedRect>) {
					changed |= DrawGraphicsRoundedRect(
						ctx,
						active,
						convert,
						command_index
					);
				} else if constexpr (std::same_as<Value, Polygon>) {
					changed |= DrawPolygonContents(
						ctx,
						active,
						convert,
						[&ctx, command_index](std::size_t vertex_index) {
							return MakeGraphicsPositionApply<Polygon>(
								ctx,
								command_index,
								[vertex_index](Polygon& value) -> V2_float& {
									return value.vertices[vertex_index];
								}
							);
						}
					);
				} else if constexpr (std::same_as<Value, Line>) {
					changed |= DrawGraphicsLine<Line>(
						ctx,
						active,
						convert,
						command_index
					);
				} else if constexpr (std::same_as<Value, Capsule>) {
					changed |= DrawGraphicsCapsule(
						ctx,
						active,
						convert,
						command_index
					);
				} else if constexpr (std::same_as<Value, Triangle>) {
					changed |= DrawGraphicsTriangle(
						ctx,
						active,
						convert,
						command_index
					);
				} else if constexpr (std::same_as<Value, Circle>) {
					changed |= DrawRadiusValue(ctx, active.radius);
				} else if constexpr (std::same_as<Value, Arc>) {
					changed |= DrawGraphicsArc(ctx, active);
				} else {
					changed |= DrawDefaultContents(ctx, active);
				}
			}
		},
		shape
	);

	return changed;
}

bool DrawGraphicsShape(
	EditorContext& ctx,
	Shape& shape,
	const PositionPicker::Convert& convert,
	std::size_t command_index
) {
	auto member{ ReflectValue(shape) };
	return DrawGraphicsShape(
		ctx,
		member.value,
		convert,
		command_index
	);
}

bool DrawGraphicsCommand(
	EditorContext& ctx,
	::ptgn::impl::GraphicsCommand& command,
	std::size_t command_index
) {
	bool changed{ false };

	changed |= DrawMembers(
		ctx,
		command.transform
	);
	const PositionPicker::Convert convert{
		MakeInspectorPositionConverter(
			ctx,
			command.transform
		)
	};

	changed |= DrawGraphicsShape(
		ctx,
		command.shape,
		convert,
		command_index
	);
	changed |= ForceGraphicsLineStyleHollow(command);
	changed |= DrawValue(
		ctx,
		"Color",
		command.color
	);
	changed |= DrawFillStyle(
		ctx,
		"Style",
		command.line_width
	);
	changed |= ForceGraphicsLineStyleHollow(command);

	return changed;
}

bool DrawGraphicsDataContents(
	EditorContext& ctx,
	::ptgn::impl::GraphicsData& graphics
) {
	ScopedDisabled disabled{ IsPositionPickingActive(ctx) };

	return DrawVectorEditor(
		graphics.commands_,
		VectorOptions{
			.item_name = "Command",
			.add_label = "+ Command",
			.default_open = false,
			.reorderable = true,
			.add_first = true,
		},
		[&ctx](
			::ptgn::impl::GraphicsCommand& command,
			std::size_t command_index
		) {
			return DrawGraphicsCommand(
				ctx,
				command,
				command_index
			);
		}
	);
}

template <>
struct ComponentDrawer<::ptgn::impl::GraphicsData> {
	static bool Draw(
		EditorContext& ctx,
		::ptgn::impl::GraphicsData& graphics
	) {
		return DrawGraphicsDataContents(ctx, graphics);
	}
};

template <>
struct ComponentDrawer<Polygon> {
	static bool Draw(EditorContext& ctx, Polygon& polygon) {
		return DrawPolygonContents(
			ctx,
			polygon,
			MakeInspectorPositionConverter(ctx),
			[&ctx](std::size_t vertex_index) {
				return MakeComponentPositionApply<Polygon>(
					ctx,
					[vertex_index](Polygon& value) -> V2_float& {
						return value.vertices[vertex_index];
					}
				);
			}
		);
	}
};

template <>
struct ComponentDrawer<ButtonBorderVisuals> {
	static bool Draw(EditorContext& ctx, ButtonBorderVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct ComponentDrawer<ButtonBackgroundVisuals> {
	static bool Draw(EditorContext& ctx, ButtonBackgroundVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct ComponentDrawer<ButtonTextVisuals> {
	static bool Draw(EditorContext& ctx, ButtonTextVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct ComponentDrawer<ButtonSpriteVisuals> {
	static bool Draw(EditorContext& ctx, ButtonSpriteVisuals& visuals) {
		return DrawEnumArrayEditor<ButtonVisualState>(ctx, visuals.states);
	}
};

template <>
struct ComponentDrawer<ButtonSounds> {
	static bool Draw(EditorContext& ctx, ButtonSounds& sounds) {
		bool changed{ false };
		changed |= DrawEnumArrayEditor<ButtonVisualState>(ctx, "Sounds", sounds.states);
		changed |= DrawValue(ctx, "Exclusive Audio", sounds.exclusive);
		return changed;
	}
};

namespace {

using ErasedValueDrawer = bool (*)(EditorContext& ctx, std::string_view label, const ReflectedComponentMember& member);
using ErasedComponentDrawer = bool (*)(EditorContext& ctx, void* value, bool read_only);

template <typename T>
bool DrawTypedValue(
	EditorContext& ctx,
	std::string_view label,
	const ReflectedComponentMember& member
) {
	if (!member.value) {
		return false;
	}

	if (member.mutable_value && !member.read_only) {
		return DrawValue(ctx, label, *static_cast<T*>(member.mutable_value));
	}

	return DrawReadOnlyValue(ctx, label, *static_cast<const T*>(member.value));
}

template <typename T>
bool DrawTypedComponent(EditorContext& ctx, void* value, bool read_only) {
	if (!value) {
		return false;
	}

	if (!read_only) {
		return ComponentDrawer<T>::Draw(ctx, *static_cast<T*>(value));
	}

	if constexpr (std::copy_constructible<T>) {
		T copy{ *static_cast<const T*>(value) };
		ReadOnlyScope scope{ true };
		ComponentDrawer<T>::Draw(ctx, copy);
	}

	return false;
}

template <typename T>
ErasedComponentDrawer GetTypedComponentDrawer() {
	if constexpr (kHasComponentDrawer<T>) {
		return &DrawTypedComponent<T>;
	} else {
		return nullptr;
	}
}

ErasedValueDrawer FindKnownValueDrawer(std::size_t type_id) {
#define PTGN_INSPECTOR_VALUE_DRAWER(Type) \
	if (type_id == Hash<Type>()) {          \
		return &DrawTypedValue<Type>;          \
	}

	PTGN_INSPECTOR_VALUE_DRAWER(Color)
	PTGN_INSPECTOR_VALUE_DRAWER(FillStyle)
	PTGN_INSPECTOR_VALUE_DRAWER(V2_float)
	PTGN_INSPECTOR_VALUE_DRAWER(V3_float)
	PTGN_INSPECTOR_VALUE_DRAWER(V4_float)
	PTGN_INSPECTOR_VALUE_DRAWER(V2_int)
	PTGN_INSPECTOR_VALUE_DRAWER(V3_int)
	PTGN_INSPECTOR_VALUE_DRAWER(V4_int)
	PTGN_INSPECTOR_VALUE_DRAWER(Degrees)
	PTGN_INSPECTOR_VALUE_DRAWER(Radians)
	PTGN_INSPECTOR_VALUE_DRAWER(FontStyle)
	PTGN_INSPECTOR_VALUE_DRAWER(Matrix4)
	PTGN_INSPECTOR_VALUE_DRAWER(TextureKey)
	PTGN_INSPECTOR_VALUE_DRAWER(FontKey)
	PTGN_INSPECTOR_VALUE_DRAWER(AudioKey)
	PTGN_INSPECTOR_VALUE_DRAWER(ShaderKey)
	PTGN_INSPECTOR_VALUE_DRAWER(JsonKey)

#undef PTGN_INSPECTOR_VALUE_DRAWER
	return nullptr;
}

ErasedComponentDrawer FindCustomDrawer(std::size_t type_id) {
#define PTGN_INSPECTOR_COMPONENT_DRAWER(Type) \
	if (type_id == Hash<Type>()) {              \
		return GetTypedComponentDrawer<Type>();   \
	}

	PTGN_INSPECTOR_COMPONENT_DRAWER(Transform)
	PTGN_INSPECTOR_COMPONENT_DRAWER(TextBox)
	PTGN_INSPECTOR_COMPONENT_DRAWER(StyledText)
	PTGN_INSPECTOR_COMPONENT_DRAWER(AnimationConfig)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::Scripts)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ParentRenderTarget)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TextData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::GraphicsData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ParticleEmitterData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(LightData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ShadowCaster)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IDrawable)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Depth)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Visible)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Tint)
	PTGN_INSPECTOR_COMPONENT_DRAWER(FillStyle)
	PTGN_INSPECTOR_COMPONENT_DRAWER(BlendMode)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Color)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Origin)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Rect)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Circle)
	PTGN_INSPECTOR_COMPONENT_DRAWER(RoundedRect)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Polygon)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Ellipse)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Triangle)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Line)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Capsule)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Arc)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::Interactive)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::Draggable)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::Dropzone)
	PTGN_INSPECTOR_COMPONENT_DRAWER(InteractionLock)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::InteractiveTag)

	PTGN_INSPECTOR_COMPONENT_DRAWER(Collider)
	PTGN_INSPECTOR_COMPONENT_DRAWER(RigidBody)
	PTGN_INSPECTOR_COMPONENT_DRAWER(BoundaryBehavior)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Lifetime)
	PTGN_INSPECTOR_COMPONENT_DRAWER(TopDownMovement)
	PTGN_INSPECTOR_COMPONENT_DRAWER(PlatformerMovement)
	PTGN_INSPECTOR_COMPONENT_DRAWER(PlatformerJump)

	PTGN_INSPECTOR_COMPONENT_DRAWER(TextureKey)
	PTGN_INSPECTOR_COMPONENT_DRAWER(FontKey)
	PTGN_INSPECTOR_COMPONENT_DRAWER(AudioKey)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ShaderKey)
	PTGN_INSPECTOR_COMPONENT_DRAWER(JsonKey)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ButtonData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ButtonAnimationPart)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ButtonBackgroundVisuals)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ButtonBorderVisuals)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ButtonSpriteVisuals)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ButtonTextVisuals)
	PTGN_INSPECTOR_COMPONENT_DRAWER(ButtonSounds)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ToggleButtonData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ToggleButtonGroupData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ToggleButtonGroupItem)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::DropdownData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::DropdownItem)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TooltipData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TooltipHoverData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TooltipBackgroundPart)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TooltipTextPart)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderTrackData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderThumbData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderTrackBackgroundData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderTrackBorderData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderTrackSpriteData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::SliderValueTextData)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentOffset)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentImmovable)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentTransform)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentPosition)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentRotation)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentScale)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentDepth)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentVisibility)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::IgnoreParentTint)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::EffectTag)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::HDREffectTag)
	PTGN_INSPECTOR_COMPONENT_DRAWER(EffectMargin)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Bloom)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Blur)
	PTGN_INSPECTOR_COMPONENT_DRAWER(GaussianBlur)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::RenderTargetDesc)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::RenderMask)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::CameraMask)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::CameraData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ClearColor)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ClearDepth)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::ClearStencil)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::UILayer)
	PTGN_INSPECTOR_COMPONENT_DRAWER(Material)

	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TextureSize)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::TextureCrop)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::AnimationData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(SpriteStackData)
	PTGN_INSPECTOR_COMPONENT_DRAWER(::ptgn::impl::Offsets)

#undef PTGN_INSPECTOR_COMPONENT_DRAWER
	return nullptr;
}

struct ReflectionFrame {
	ComponentReflectionNodeKind kind{ ComponentReflectionNodeKind::Value };
	bool visit_children{ true };
	bool tree_open{ false };
	bool indented{ false };
	bool custom{ false };

	void* sequence{ nullptr };
	ReflectedComponentSequenceInsertCallback insert{ nullptr };
	ReflectedComponentSequenceEraseCallback erase{ nullptr };
	ReflectedComponentSequenceMoveCallback move{ nullptr };
	std::optional<std::size_t> remove_index{};
	std::optional<std::pair<std::size_t, std::size_t>> move_indices{};
	bool add_item{ false };
};

struct ReflectionDrawState {
	EditorContext& ctx;
	bool changed{ false };
	std::vector<ReflectionFrame> frames{};
};

[[nodiscard]] std::string ReflectionLabel(std::string_view name) {
	return name.empty() ? std::string{} : PrettyName(name);
}

[[nodiscard]] bool ShouldShowReflectionMember(
	const ReflectionDrawState& state,
	const ReflectedComponentMember& member
) {
	return !member.read_only || state.ctx.local.settings.show_read_only_inspector_data;
}

bool DrawGenericScalar(
	ReflectionDrawState& state,
	const ReflectedComponentMember& member
) {
	if (!ShouldShowReflectionMember(state, member)) {
		return false;
	}

	const std::string label{ ReflectionLabel(member.name) };

	if (auto drawer{ FindKnownValueDrawer(member.type_id) }) {
		return drawer(state.ctx, label, member);
	}

	const bool disabled{ member.read_only || !member.mutable_value };

	switch (member.value_kind) {
		case ComponentReflectionValueKind::Bool: {
			bool value{ member.bool_value };
			const bool changed{ DrawPropertyRow(label, [&]() {
				ScopedDisabled scope{ disabled };
				return ImGui::Checkbox("##value", &value);
			}) };
			return changed && member.set_bool && member.set_bool(member.mutable_value, value);
		}
		case ComponentReflectionValueKind::SignedInteger: {
			std::int64_t value{ member.signed_value };
			const bool changed{ DrawPropertyRow(label, [&]() {
				ScopedDisabled scope{ disabled };
				return ImGui::DragScalar("##value", ImGuiDataType_S64, &value, 1.0f);
			}) };
			return changed && member.set_signed && member.set_signed(member.mutable_value, value);
		}
		case ComponentReflectionValueKind::UnsignedInteger: {
			std::uint64_t value{ member.unsigned_value };
			const bool changed{ DrawPropertyRow(label, [&]() {
				ScopedDisabled scope{ disabled };
				return ImGui::DragScalar("##value", ImGuiDataType_U64, &value, 1.0f);
			}) };
			return changed && member.set_unsigned && member.set_unsigned(member.mutable_value, value);
		}
		case ComponentReflectionValueKind::FloatingPoint: {
			double value{ member.floating_value };
			const bool changed{ DrawPropertyRow(label, [&]() {
				ScopedDisabled scope{ disabled };
				return ImGui::DragScalar("##value", ImGuiDataType_Double, &value, 0.1f);
			}) };
			return changed && member.set_float && member.set_float(member.mutable_value, value);
		}
		case ComponentReflectionValueKind::String: {
			if (!member.value) {
				return false;
			}
			if (member.mutable_value && !member.read_only) {
				return DrawValue(
					state.ctx, label, *static_cast<std::string*>(member.mutable_value)
				);
			}
			return DrawReadOnlyValue(
				state.ctx, label, *static_cast<const std::string*>(member.value)
			);
		}
		case ComponentReflectionValueKind::Enum: {
			const std::size_t selected{ member.enum_index };
			const std::string_view preview{
				member.enum_name && member.enum_count > 0
					? member.enum_name(selected)
					: std::string_view{ "Unknown" }
			};
			bool changed{ false };
			DrawPropertyRow(label, [&]() {
				ScopedDisabled scope{ disabled };
				if (ImGui::BeginCombo("##value", preview.data())) {
					for (std::size_t index{ 0 }; index < member.enum_count; ++index) {
						const auto name{ member.enum_name(index) };
						if (ImGui::Selectable(name.data(), index == selected)) {
							changed = member.enum_set && member.enum_set(member.mutable_value, index);
						}
					}
					ImGui::EndCombo();
				}
				return changed;
			});
			return changed;
		}
		case ComponentReflectionValueKind::Unknown: break;
	}

	ImGui::TextDisabled("%s: <unsupported>", label.c_str());
	return false;
}

ReflectionFrame* FindParentSequenceFrame(ReflectionDrawState& state) {
	for (auto it{ state.frames.rbegin() }; it != state.frames.rend(); ++it) {
		if (it->kind == ComponentReflectionNodeKind::BeginSequence) {
			return std::addressof(*it);
		}
	}
	return nullptr;
}

void DrawReflectionNode(void* user_data, const ReflectedComponentMember& member) {
	auto& state{ *static_cast<ReflectionDrawState*>(user_data) };
	const void* node_id{
		member.value
			? member.value
			: member.mutable_value
	};
	ScopedID node_scope{
		node_id
			? node_id
			: static_cast<const void*>(std::addressof(member))
	};

	switch (member.kind) {
		case ComponentReflectionNodeKind::Value:
			if (auto custom_drawer{ FindCustomDrawer(member.type_id) }) {
				if (ShouldShowReflectionMember(state, member)) {
					state.changed |= custom_drawer(
						state.ctx,
						member.mutable_value ? member.mutable_value : const_cast<void*>(member.value),
						member.read_only
					);
				}
			} else {
				state.changed |= DrawGenericScalar(state, member);
			}
			break;

		case ComponentReflectionNodeKind::BeginObject: {
			ReflectionFrame frame{ .kind = member.kind };
			if (!ShouldShowReflectionMember(state, member)) {
				frame.visit_children = false;
				state.frames.push_back(frame);
				break;
			}

			const bool root{ member.depth == 0 && member.name.empty() };

			if (auto value_drawer{ FindKnownValueDrawer(member.type_id) }) {
				state.changed |= value_drawer(state.ctx, ReflectionLabel(member.name), member);
				frame.visit_children = false;
				frame.custom = true;
				state.frames.push_back(frame);
				break;
			}

			if (auto custom_drawer{ FindCustomDrawer(member.type_id) }) {
				frame.visit_children = false;
				frame.custom = true;

				if (root) {
					state.changed |= custom_drawer(
						state.ctx,
						member.mutable_value ? member.mutable_value : const_cast<void*>(member.value),
						member.read_only
					);
				} else {
					const std::string label{ ReflectionLabel(member.name) };
					frame.tree_open = ImGui::TreeNodeEx(
						label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth
					);
					if (frame.tree_open) {
						ScopedIndent indent;
						state.changed |= custom_drawer(
							state.ctx,
							member.mutable_value ? member.mutable_value : const_cast<void*>(member.value),
							member.read_only
						);
						ImGui::TreePop();
						frame.tree_open = false;
					}
				}

				state.frames.push_back(frame);
				break;
			}

			if (!root) {
				const std::string label{ ReflectionLabel(member.name) };
				frame.tree_open = ImGui::TreeNodeEx(
					label.c_str(), ImGuiTreeNodeFlags_SpanAvailWidth
				);
				frame.visit_children = frame.tree_open;
				if (frame.tree_open) {
					ImGui::Indent();
					frame.indented = true;
				}
			}
			state.frames.push_back(frame);
			break;
		}

		case ComponentReflectionNodeKind::EndObject: {
			if (state.frames.empty()) {
				break;
			}
			auto frame{ state.frames.back() };
			state.frames.pop_back();
			if (frame.indented) {
				ImGui::Unindent();
			}
			if (frame.tree_open) {
				ImGui::TreePop();
			}
			break;
		}

		case ComponentReflectionNodeKind::BeginOptional: {
			ReflectionFrame frame{ .kind = member.kind };
			if (!ShouldShowReflectionMember(state, member)) {
				frame.visit_children = false;
				state.frames.push_back(frame);
				break;
			}

			bool enabled{ member.optional_has_value };
			const bool changed{
				DrawOptionalLabelRow(
					ReflectionLabel(member.name),
					enabled,
					member.read_only ||
						!member.mutable_value ||
						!member.optional_set
				)
			};
			if (changed && member.optional_set) {
				state.changed |= member.optional_set(member.mutable_value, enabled);
			}
			frame.visit_children = enabled;
			if (frame.visit_children) {
				ImGui::Indent();
				frame.indented = true;
			}
			state.frames.push_back(frame);
			break;
		}

		case ComponentReflectionNodeKind::EndOptional: {
			if (!state.frames.empty()) {
				auto frame{ state.frames.back() };
				state.frames.pop_back();
				if (frame.indented) {
					ImGui::Unindent();
				}
			}
			break;
		}

		case ComponentReflectionNodeKind::BeginSequence: {
			ReflectionFrame frame{
				.kind = member.kind,
				.sequence = member.mutable_value,
				.insert = member.sequence_insert,
				.erase = member.sequence_erase,
				.move = member.sequence_move,
			};
			if (!ShouldShowReflectionMember(state, member)) {
				frame.visit_children = false;
				state.frames.push_back(frame);
				break;
			}

			const std::string header{
				ReflectionLabel(member.name) + " (" + std::to_string(member.sequence_size) + ")"
			};
			frame.tree_open = ImGui::TreeNodeEx(
				header.c_str(),
				ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen
			);
			frame.visit_children = frame.tree_open;
			if (frame.tree_open) {
				ImGui::Indent();
				frame.indented = true;
			}
			state.frames.push_back(frame);
			break;
		}

		case ComponentReflectionNodeKind::EndSequence: {
			if (state.frames.empty()) {
				break;
			}
			auto frame{ state.frames.back() };
			state.frames.pop_back();

			if (frame.tree_open && frame.insert && frame.sequence) {
				if (ImGui::Button("Add Item", ImVec2{ -FLT_MIN, 0.0f })) {
					frame.add_item = true;
				}
			}
			if (frame.indented) {
				ImGui::Unindent();
			}
			if (frame.tree_open) {
				ImGui::TreePop();
			}

			if (frame.sequence) {
				if (frame.move_indices && frame.move) {
					state.changed |= frame.move(
						frame.sequence, frame.move_indices->first, frame.move_indices->second
					);
				} else if (frame.remove_index && frame.erase) {
					state.changed |= frame.erase(frame.sequence, *frame.remove_index);
				} else if (frame.add_item && frame.insert) {
					state.changed |= frame.insert(frame.sequence, member.sequence_size);
				}
			}
			break;
		}

		case ComponentReflectionNodeKind::BeginSequenceElement: {
			ReflectionFrame frame{ .kind = member.kind };
			auto* sequence{ FindParentSequenceFrame(state) };
			if (!sequence || !ShouldShowReflectionMember(state, member)) {
				frame.visit_children = false;
				state.frames.push_back(frame);
				break;
			}

			const std::size_t index{ member.sequence_index };
			const float spacing{ ImGui::GetStyle().ItemInnerSpacing.x };
			const ImVec2 button_size{ ImGui::GetFrameHeight(), ImGui::GetFrameHeight() };
			const float actions_width{ button_size.x * 3.0f + spacing * 2.0f };
			const float header_width{ std::max(1.0f, ImGui::GetContentRegionAvail().x - actions_width - spacing) };
			const std::string label{ "Item " + std::to_string(index + 1) };
			frame.tree_open = DrawFixedWidthCollapsingHeader(label, header_width, true);
			frame.visit_children = frame.tree_open;

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled up_disabled{ member.read_only || index == 0 };
				if (ImGui::ArrowButton("##up", ImGuiDir_Up)) {
					sequence->move_indices = std::pair{ index, index - 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled down_disabled{ member.read_only || index + 1 >= member.sequence_size };
				if (ImGui::ArrowButton("##down", ImGuiDir_Down)) {
					sequence->move_indices = std::pair{ index, index + 1 };
				}
			}

			ImGui::SameLine(0.0f, spacing);
			{
				ScopedDisabled remove_disabled{ member.read_only };
				if (ImGui::Button("X##remove", button_size)) {
					sequence->remove_index = index;
				}
			}

			if (frame.tree_open) {
				ImGui::Indent();
				frame.indented = true;
			}
			state.frames.push_back(frame);
			break;
		}

		case ComponentReflectionNodeKind::EndSequenceElement: {
			if (!state.frames.empty()) {
				auto frame{ state.frames.back() };
				state.frames.pop_back();
				if (frame.indented) {
					ImGui::Unindent();
				}
			}
			break;
		}

		case ComponentReflectionNodeKind::BeginVariant: {
			ReflectionFrame frame{ .kind = member.kind };
			if (!ShouldShowReflectionMember(state, member)) {
				frame.visit_children = false;
				state.frames.push_back(frame);
				break;
			}

			const std::string_view preview{
				member.variant_name ? member.variant_name(member.variant_index) : std::string_view{ "Unknown" }
			};
			bool variant_changed{ false };
			DrawPropertyRow(ReflectionLabel(member.name), [&]() {
				ScopedDisabled disabled{ member.read_only || !member.mutable_value || !member.variant_set };
				if (ImGui::BeginCombo("##value", preview.data())) {
					for (std::size_t index{ 0 }; index < member.variant_count; ++index) {
						const auto name{ member.variant_name(index) };
						if (
							ImGui::Selectable(name.data(), index == member.variant_index) &&
							index != member.variant_index
						) {
							variant_changed = member.variant_set(member.mutable_value, index);
							state.changed |= variant_changed;
						}
					}
					ImGui::EndCombo();
				}
				return variant_changed;
			});
			frame.visit_children = !variant_changed;
			if (frame.visit_children) {
				ImGui::Indent();
				frame.indented = true;
			}
			state.frames.push_back(frame);
			break;
		}

		case ComponentReflectionNodeKind::EndVariant: {
			if (!state.frames.empty()) {
				auto frame{ state.frames.back() };
				state.frames.pop_back();
				if (frame.indented) {
					ImGui::Unindent();
				}
			}
			break;
		}
	}
}

bool ShouldVisitReflectionNode(void* user_data, const ReflectedComponentMember&) {
	auto& state{ *static_cast<ReflectionDrawState*>(user_data) };
	return state.frames.empty() || state.frames.back().visit_children;
}

} // namespace

bool DrawReflectedContents(
	EditorContext& ctx,
	std::string_view label,
	void* value,
	ReflectedValueVisitCallback visit
) {
	if (!value || !visit) {
		return false;
	}

	ReflectionDrawState state{ .ctx = ctx };
	AutoLabelWidthScope label_width{ label };
	visit(
		value,
		ComponentReflectionVisitor{
			.user_data = std::addressof(state),
			.callback = &DrawReflectionNode,
			.should_visit_children = &ShouldVisitReflectionNode,
		}
	);
	return state.changed;
}

bool DrawInspectorValueContents(
	EditorContext& ctx,
	std::size_t type_id,
	void* value
) {
	if (!value) {
		return false;
	}

	if (auto drawer{ FindCustomDrawer(type_id) }) {
		return drawer(ctx, value, false);
	}

	return DrawRegisteredComponentContents(ctx, type_id, value);
}

bool DrawRegisteredComponentContents(
	EditorContext& ctx,
	std::size_t type_id,
	void* value
) {
	if (!value) {
		return false;
	}

	if (type_id == Hash<::ptgn::impl::GraphicsData>()) {
		return DrawGraphicsDataContents(
			ctx,
			*static_cast<::ptgn::impl::GraphicsData*>(value)
		);
	}

	const auto* registration{ ComponentRegistry::Find(type_id) };
	if (!registration) {
		ImGui::TextDisabled("Unregistered component type: %zu", type_id);
		return false;
	}

	ReflectionDrawState state{ .ctx = ctx };
	AutoLabelWidthScope label_width{ registration->name };
	registration->Visit(
		value,
		ComponentReflectionVisitor{
			.user_data = std::addressof(state),
			.callback = &DrawReflectionNode,
			.should_visit_children = &ShouldVisitReflectionNode,
		}
	);
	return state.changed;
}

} // namespace ptgn::editor::inspector
