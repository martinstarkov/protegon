#include "panels/inspector_features.h"
#include "panels/inspector_geometry.h"
#include "panels/rich_text_editor.h"

namespace ptgn::editor::inspector {

namespace {

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponent(
	Target& target, std::string_view label, Draw&& draw, Callback callback = nullptr
) {
	return DrawRequiredComponent<Target, T>(
		target, label, false, std::forward<Draw>(draw), callback
	);
}

template <typename Target, typename T, typename Draw, typename Callback = std::nullptr_t>
bool DrawRequiredInlineVisualComponentWithDefault(
	Target& target, std::string_view label, T default_value, Draw&& draw,
	Callback callback = nullptr
) {
	ScopedID target_scope{ target.Id() };
	ScopedID component_scope{ static_cast<int>(Hash<T>()) };

	auto before{ target.template Capture<T>() };
	bool changed{ false };

	if (!before) {
		target.template SetLive<T>(std::move(default_value), callback);
		changed = true;
	}

	T value{ target.template Capture<T>().value_or(T{}) };
	changed |= std::invoke(std::forward<Draw>(draw), value);

	if (changed) {
		target.template SetLive<T>(std::move(value), callback);
	}

	auto after{ target.template Capture<T>() };
	TrackComponentState(
		target, std::string{ "Edit " } + std::string{ label }, std::move(before), std::move(after),
		changed, callback
	);

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalVisualComponent(
	Target& target, std::string_view label, bool tree = false, bool contents_read_only = false,
	bool toggle_read_only = false
) {
	if constexpr (std::is_empty_v<T>) {
		return DrawOptionalComponent<Target, T>(
			target, label, false, [](T&) { return false; }, contents_read_only, toggle_read_only
		);
	} else {
		return DrawOptionalComponent<Target, T>(
			target, label, tree,
			[&target](T& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<T>(), std::addressof(value)
				);
			},
			contents_read_only, toggle_read_only
		);
	}
}

[[nodiscard]] bool IsShapeRenderer(std::string_view visual) {
	return visual == "rect" || visual == "circle" || visual == "roundedrect" ||
		   visual == "polygon" || visual == "ellipse" || visual == "triangle" || visual == "line" ||
		   visual == "capsule" || visual == "arc";
}

[[nodiscard]] bool IsEffectRenderer(std::string_view visual);

inline constexpr V2_float kInspectorDefaultShapeSize{ 100.0f, 100.0f };
inline constexpr float kInspectorDefaultShapeRadius{ 50.0f };

// Keep renderer selected geometry consistent with the Scene Hierarchy create menu.
template <typename T>
[[nodiscard]] T MakeDefaultShapeGeometry() {
	if constexpr (std::same_as<T, Rect>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ kInspectorDefaultShapeSize };
		}
	} else if constexpr (std::same_as<T, Circle>) {
		if constexpr (std::constructible_from<T, float>) {
			return T{ kInspectorDefaultShapeRadius };
		}
	} else if constexpr (std::same_as<T, Line>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float>) {
			return T{ V2_float{ -100.0f, -100.0f }, V2_float{ 100.0f, 100.0f } };
		}
	} else if constexpr (std::same_as<T, Polygon>) {
		std::vector<V2_float> vertices{
			{ 0.0f, -50.0f },  { 47.0f, -15.0f },  { 29.0f, 40.0f },
			{ -29.0f, 40.0f }, { -47.0f, -15.0f },
		};

		if constexpr (std::constructible_from<T, std::vector<V2_float>>) {
			return T{ std::move(vertices) };
		}
	} else if constexpr (std::same_as<T, Ellipse>) {
		if constexpr (std::constructible_from<T, V2_float>) {
			return T{ V2_float{ 100.0f, 50.0f } };
		}
	} else if constexpr (std::same_as<T, Arc>) {
		if constexpr (std::constructible_from<T, float, float, float, bool>) {
			return T{ kInspectorDefaultShapeRadius, 0.0f, 90.0f, true };
		} else if constexpr (std::constructible_from<T, float, Degrees, Degrees, bool>) {
			return T{
				kInspectorDefaultShapeRadius,
				Degrees{ 0.0f },
				Degrees{ 90.0f },
				true,
			};
		}
	} else if constexpr (std::same_as<T, RoundedRect>) {
		if constexpr (std::constructible_from<T, V2_float, float>) {
			return T{ kInspectorDefaultShapeSize, 10.0f };
		} else if constexpr (std::constructible_from<T, Rect, float>) {
			return T{ MakeDefaultShapeGeometry<Rect>(), 10.0f };
		}
	} else if constexpr (std::same_as<T, Triangle>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, V2_float>) {
			return T{
				V2_float{ -100.0f, 50.0f },
				V2_float{ 0.0f, -50.0f },
				V2_float{ 100.0f, 50.0f },
			};
		}
	} else if constexpr (std::same_as<T, Capsule>) {
		if constexpr (std::constructible_from<T, V2_float, V2_float, float>) {
			return T{
				V2_float{ -100.0f, -100.0f },
				V2_float{ 100.0f, 100.0f },
				kInspectorDefaultShapeRadius,
			};
		} else if constexpr (std::constructible_from<T, Line, float>) {
			return T{
				MakeDefaultShapeGeometry<Line>(),
				kInspectorDefaultShapeRadius,
			};
		}
	}

	return T{};
}

using RendererOwnedComponents = FeatureComponents<
	Rect, Circle, RoundedRect, Polygon, Ellipse, Triangle, Line, Capsule, Arc, Color, FillStyle,
	TextureKey, ::ptgn::SpriteStackData, ::ptgn::impl::TextureSize, ::ptgn::impl::TextureCrop,
	::ptgn::impl::AnimationData, ::ptgn::impl::Offsets, ::ptgn::impl::TextData,
	::ptgn::impl::ParticleEmitterData, LightData, ::ptgn::impl::ShadowCaster,
	::ptgn::impl::GraphicsData, ::ptgn::Material, ShaderKey, ::ptgn::impl::RenderTargetDesc,
	::ptgn::impl::EffectTag, ::ptgn::impl::HDREffectTag, EffectMargin, Bloom, Blur, GaussianBlur,
	::ptgn::impl::ClearColor, ::ptgn::impl::ClearDepth, ::ptgn::impl::ClearStencil>;

template <typename Target, typename T>
void ClearRendererOwnedComponent(Target& target) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::nullopt);
	}
}

template <typename Target, typename... T>
void ClearRendererOwnedComponents(Target& target, FeatureComponents<T...>) {
	(ClearRendererOwnedComponent<Target, T>(target), ...);
}

template <typename T, typename Target>
void SetRendererOwnedComponent(Target& target, T value = T{}) {
	if constexpr (Target::template Supports<T>()) {
		target.template SetLive<T>(std::move(value));
	}
}

[[nodiscard]] ::ptgn::impl::TextData MakeDefaultTextRendererData() {
	::ptgn::impl::TextData data;
	auto members{ ReflectMembers(data) };

	std::apply(
		[](auto&&... member) {
			(
				[&] {
					using Member = std::remove_cvref_t<decltype(member.value)>;

					if constexpr (std::same_as<Member, StyledText>) {
						if (member.value.runs.empty()) {
							member.value.runs.emplace_back();
						}
					}
				}(),
				...);
		},
		members
	);

	return data;
}

template <typename Target>
void InitializeRendererOwnedComponents(Target& target, std::string_view visual) {
	auto add_shape = [&]<typename T>() {
		SetRendererOwnedComponent<T>(target, MakeDefaultShapeGeometry<T>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ Solid{} });
	};

	if (visual == "rect") {
		add_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		add_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		add_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		add_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		add_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		add_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		SetRendererOwnedComponent<Line>(target, MakeDefaultShapeGeometry<Line>());
		SetRendererOwnedComponent<Color>(target, Color{ color::White });
		SetRendererOwnedComponent<FillStyle>(target, FillStyle{ kInspectorMinLineWidth });
	} else if (visual == "capsule") {
		add_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		add_shape.template operator()<Arc>();
	} else if (visual == "spritestack") {
		SetRendererOwnedComponent<SpriteStackData>(target);
	} else if (visual.contains("text")) {
		SetRendererOwnedComponent<::ptgn::impl::TextData>(target, MakeDefaultTextRendererData());
	} else if (visual.contains("particle")) {
		SetRendererOwnedComponent<::ptgn::impl::ParticleEmitterData>(target);
	} else if (visual.contains("light")) {
		SetRendererOwnedComponent<LightData>(target);
	} else if (visual.contains("customshader")) {
		SetRendererOwnedComponent<::ptgn::Material>(target);
	} else if (visual.contains("graphics")) {
		SetRendererOwnedComponent<::ptgn::impl::GraphicsData>(target);
	} else if (visual.contains("rendertarget")) {
		SetRendererOwnedComponent<::ptgn::impl::RenderTargetDesc>(target);
	}

	if (visual.contains("gaussianblur")) {
		SetRendererOwnedComponent<GaussianBlur>(target);
	} else if (visual.contains("blur")) {
		SetRendererOwnedComponent<Blur>(target);
	} else if (visual.contains("bloom")) {
		SetRendererOwnedComponent<Bloom>(target);
	}
}

template <typename Target>
void ApplyRendererSelection(Target& target, ComponentState<::ptgn::impl::IDrawable> drawable) {
	ClearRendererOwnedComponents(target, RendererOwnedComponents{});
	target.template SetLive<::ptgn::impl::IDrawable>(drawable);

	if (!drawable) {
		return;
	}

	const auto* info{ ::ptgn::impl::IDrawable::FindInfo(drawable->hash) };

	if (!info) {
		return;
	}

	InitializeRendererOwnedComponents(
		target, NormalizeFeatureName(GetDrawableInspectorLabel(*info))
	);
}

struct RendererRowResult {
	std::string visual{};
	bool changed{ false };
};

template <typename Target>
RendererRowResult DrawRendererRow(Target& target) {
	using Drawable = ::ptgn::impl::IDrawable;

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	auto drawable{ target.template Capture<Drawable>() };
	auto before_visible{ target.template Capture<Visible>() };

	bool visible{ before_visible ? before_visible->visible : true };

	const auto* info{ drawable ? Drawable::FindInfo(drawable->hash) : nullptr };

	const std::string preview{ primary_scene_target ? "Render Target"
							   : info				? GetDrawableInspectorLabel(*info)
													: "None" };

	const float start_x{ ImGui::GetCursorPosX() };
	const float checkbox_slot{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x };

	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted("Renderer");
	MeasurePropertyLabel("Renderer", start_x + checkbox_slot);
	ImGui::SameLine();
	ImGui::SetCursorPosX(GetPropertyValueX(start_x));

	const float spacing{ ImGui::GetStyle().ItemSpacing.x };
	const float visible_width{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x +
							   ImGui::CalcTextSize("Visible").x };
	const float combo_width{
		std::max(80.0f, ImGui::GetContentRegionAvail().x - visible_width - spacing)
	};

	bool renderer_changed{ false };
	auto before_renderer{
		CaptureInspectorFeatureState(target, InspectorFeature::Visual, VisualFeatureComponents{})
	};

	auto choose_renderer = [&](ComponentState<Drawable> selected) {
		const bool same_renderer{ selected.has_value() == drawable.has_value() &&
								  (!selected || selected->hash == drawable->hash) };

		if (same_renderer) {
			return;
		}

		ApplyRendererSelection(target, selected);

		if (!selected) {
			SetFeatureManuallyAdded(target.GetFeatureTargetKey(), InspectorFeature::Visual, true);
		}

		drawable		 = target.template Capture<Drawable>();
		renderer_changed = true;
	};

	std::size_t renderer_popup_items{ 3 };
	for (const auto& candidate : Drawable::data()) {
		const std::string visual{ NormalizeFeatureName(GetDrawableInspectorLabel(candidate)) };
		if (!IsShapeRenderer(visual) && !IsEffectRenderer(visual)) {
			++renderer_popup_items;
		}
	}

	const float renderer_popup_height{ ImGui::GetStyle().WindowPadding.y * 2.0f +
									   static_cast<float>(renderer_popup_items) *
										   ImGui::GetTextLineHeightWithSpacing() -
									   ImGui::GetStyle().ItemSpacing.y };

	ImGui::SetNextItemWidth(combo_width);

	ImGui::BeginDisabled(primary_scene_target);

	ImGui::SetNextWindowSizeConstraints(
		ImVec2{ 0.0f, renderer_popup_height }, ImVec2{ FLT_MAX, renderer_popup_height }
	);

	if (ImGui::BeginCombo("##RendererSelector", preview.c_str())) {
		if (!primary_scene_target) {
			if (ImGui::Selectable("None", !drawable.has_value())) {
				choose_renderer(std::nullopt);
			}

			auto draw_candidate = [&](const auto& candidate) {
				ScopedID candidate_scope{ static_cast<const void*>(std::addressof(candidate)) };
				const std::string label{ GetDrawableInspectorLabel(candidate) };
				const bool selected{ drawable && drawable->hash == candidate.hash };

				if (ImGui::Selectable(label.c_str(), selected)) {
					choose_renderer(Drawable{ candidate.hash });
				}

				if (selected) {
					ImGui::SetItemDefaultFocus();
				}
			};

			for (const auto& candidate : Drawable::data()) {
				const std::string visual{
					NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
				};

				if (!IsShapeRenderer(visual) && !IsEffectRenderer(visual)) {
					draw_candidate(candidate);
				}
			}

			if (ImGui::BeginMenu("Shapes")) {
				for (const auto& candidate : Drawable::data()) {
					const std::string visual{
						NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
					};

					if (IsShapeRenderer(visual)) {
						draw_candidate(candidate);
					}
				}

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Effects")) {
				for (const auto& candidate : Drawable::data()) {
					const std::string visual{
						NormalizeFeatureName(GetDrawableInspectorLabel(candidate))
					};

					if (IsEffectRenderer(visual)) {
						draw_candidate(candidate);
					}
				}

				ImGui::EndMenu();
			}
		}

		ImGui::EndCombo();
	}

	ImGui::EndDisabled();

	if (primary_scene_target && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
		ImGui::SetTooltip("The scene render target renderer cannot be changed.");
	}

	if (renderer_changed) {
		auto after_renderer{ CaptureInspectorFeatureState(
			target, InspectorFeature::Visual, VisualFeatureComponents{}
		) };

		TrackInspectorFeatureState(
			target, InspectorFeature::Visual, "Change Renderer", std::move(before_renderer),
			std::move(after_renderer), VisualFeatureComponents{}
		);
	}

	bool visible_changed{ false };

	if (drawable || primary_scene_target) {
		ImGui::SameLine(0.0f, spacing);
		visible_changed = ImGui::Checkbox("Visible##RendererVisible", &visible);
	}

	if (visible_changed) {
		Visible updated{ before_visible.value_or(Visible{}) };
		updated.visible = visible;
		target.template SetLive<Visible>(ComponentState<Visible>{ updated });
	}

	auto after_visible{ target.template Capture<Visible>() };

	TrackComponentState(
		target, "Toggle Visibility", std::move(before_visible), std::move(after_visible),
		visible_changed
	);

	const auto* selected_info{ drawable ? Drawable::FindInfo(drawable->hash) : nullptr };

	return RendererRowResult{
		.visual	 = primary_scene_target ? "rendertarget"
				 : selected_info ? NormalizeFeatureName(GetDrawableInspectorLabel(*selected_info))
								 : std::string{},
		.changed = renderer_changed,
	};
}

template <typename Target>
bool DrawEffectMargin(Target& target) {
	return DrawOptionalComponent<Target, EffectMargin>(
		target, "Effect Margin", false, [&target](EffectMargin& margin) {
			const FieldOptions options{
				.speed = 1.0f,
				.min   = 0.0,
				.max   = 4096.0,
				.flags = ImGuiSliderFlags_AlwaysClamp,
			};

			return [&target, &options]<typename Margin>(Margin& value) {
				if constexpr (ReflectedValue<Margin>) {
					auto member{ ReflectValue(value) };
					return DrawValue(target.ctx, "Effect Margin", member.value, options);
				} else if constexpr (ReflectedMembers<Margin>) {
					bool changed{ false };
					auto members{ ReflectMembers(value) };

					std::apply(
						[&](auto&&... member) {
							((changed |= DrawValue(
								  target.ctx, PrettyName(member.name), member.value, options
							  )),
							 ...);
						},
						members
					);

					return changed;
				} else {
					return DrawDefaultContents(target.ctx, value);
				}
			}(margin);
		}
	);
}

[[nodiscard]] bool IsEffectRenderer(std::string_view visual) {
	if (visual.empty()) {
		return false;
	}

	const bool standard_renderer{
		visual == "rect" || visual == "circle" || visual == "roundedrect" || visual == "polygon" ||
		visual == "ellipse" || visual == "triangle" || visual == "line" || visual == "capsule" ||
		visual == "arc" || visual.contains("sprite") || visual.contains("text") ||
		visual.contains("particle") || visual.contains("light") || visual.contains("graphics") ||
		visual.contains("customshader") || visual.contains("rendertarget")
	};

	return !standard_renderer;
}

template <typename T>
bool DrawFlattenedConfig(EditorContext& ctx, T& value);

template <typename Target, typename Effect>
bool DrawSelectedEffectComponent(Target& target, std::string_view label) {
	if constexpr (!Target::template Supports<Effect>()) {
		return false;
	} else {
		return DrawRequiredComponent<Target, Effect>(
			target, label, false,
			[&target](Effect& value) { return DrawFlattenedConfig(target.ctx, value); }
		);
	}
}

template <typename Target>
bool DrawVisualEffects(Target& target, std::string_view visual) {
	bool changed{ false };

	if (visual.contains("gaussianblur")) {
		changed |= DrawSelectedEffectComponent<Target, GaussianBlur>(target, "Gaussian Blur");
	} else if (visual.contains("blur")) {
		changed |= DrawSelectedEffectComponent<Target, Blur>(target, "Blur");
	} else if (visual.contains("bloom")) {
		changed |= DrawSelectedEffectComponent<Target, Bloom>(target, "Bloom");
	}

	if (IsEffectRenderer(visual)) {
		changed |= DrawEffectMargin(target);
	}

	return changed;
}

inline bool IsTextWrapSettingName(std::string_view normalized) {
	return normalized == "allowwordbreakinoverflow" || normalized == "inserthyphenonsplit" ||
		   normalized == "preventsinglelettersplit" || normalized == "requirethreeletterremainder";
}

inline std::string TextWrapSettingLabel(std::string_view normalized) {
	if (normalized == "allowwordbreakinoverflow") {
		return "Break Overflowing Words";
	}
	if (normalized == "inserthyphenonsplit") {
		return "Insert Hyphen on Split";
	}
	if (normalized == "preventsinglelettersplit") {
		return "Prevent Single-Letter Split";
	}
	if (normalized == "requirethreeletterremainder") {
		return "Require Three-Letter Remainder";
	}
	return PrettyName(normalized);
}

inline const char* TextWrapSettingTooltip(std::string_view normalized) {
	if (normalized == "allowwordbreakinoverflow") {
		return "Split words that cannot fit on an empty line.";
	}
	if (normalized == "inserthyphenonsplit") {
		return "Insert a hyphen when a word is split.";
	}
	if (normalized == "preventsinglelettersplit") {
		return "Avoid leaving one letter behind when splitting.";
	}
	if (normalized == "requirethreeletterremainder") {
		return "Keep at least three letters on the next line when possible.";
	}
	return nullptr;
}

template <typename T, typename F>
void ForEachTextWrapMode(T& value, bool plain_mode_name, F&& fn) {
	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };
		auto visit = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::is_enum_v<Member>) {
				if (normalized == "wrapmode" || (plain_mode_name && normalized == "mode")) {
					fn(member.value);
				}
			} else if constexpr (ReflectedMembers<Member>) {
				if (normalized == "wrap") {
					ForEachTextWrapMode(member.value, true, fn);
				}
			}
		};
		std::apply([&](auto&&... member) { (visit(member), ...); }, members);
	}
}

template <typename Wrap>
bool DrawTextWrapMode(EditorContext& ctx, Wrap& wrap, bool plain_mode_name) {
	bool changed{ false };
	bool drawn{ false };

	if constexpr (std::is_enum_v<std::remove_cvref_t<Wrap>>) {
		return DrawValue(ctx, "Wrap Mode", wrap);
	} else {
		ForEachTextWrapMode(wrap, plain_mode_name, [&](auto& mode) {
			if (!drawn) {
				changed |= DrawValue(ctx, "Wrap Mode", mode);
				drawn	 = true;
			}
		});
	}

	return changed;
}

template <typename T, typename F>
void ForEachTextWrapSetting(T& value, bool inside_wrap, F&& fn) {
	if constexpr (ReflectedMembers<T>) {
		auto members{ ReflectMembers(value) };
		auto visit = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, bool>) {
				if (IsTextWrapSettingName(normalized)) {
					fn(member.value, normalized);
				}
			} else if constexpr (ReflectedMembers<Member>) {
				if (inside_wrap || normalized == "wrap") {
					ForEachTextWrapSetting(member.value, true, fn);
				}
			}
		};
		std::apply([&](auto&&... member) { (visit(member), ...); }, members);
	}
}

template <typename Wrap>
bool DrawTextWrapSettings(EditorContext&, Wrap& wrap, bool inside_wrap) {
	if constexpr (!ReflectedMembers<Wrap>) {
		return false;
	} else {
		std::vector<std::string> selected;
		ForEachTextWrapSetting(wrap, inside_wrap, [&](bool& value, std::string_view normalized) {
			if (value) {
				selected.push_back(TextWrapSettingLabel(normalized));
			}
		});

		std::string preview;
		for (const auto& item : selected) {
			if (!preview.empty()) {
				preview += ", ";
			}
			preview += item;
		}
		if (preview.empty()) {
			preview = "None";
		}

		return DrawPropertyRow("Wrap Settings", [&]() {
			bool changed{ false };
			if (ImGui::BeginCombo("##WrapSettings", preview.c_str())) {
				ForEachTextWrapSetting(
					wrap, inside_wrap, [&](bool& value, std::string_view normalized) {
						const std::string item_label{ TextWrapSettingLabel(normalized) };
						if (ImGui::Selectable(
								item_label.c_str(), value, ImGuiSelectableFlags_DontClosePopups
							)) {
							value	= !value;
							changed = true;
						}
						DrawTooltip(TextWrapSettingTooltip(normalized));
					}
				);
				ImGui::EndCombo();
			}
			return changed;
		});
	}
}

template <typename Alignment>
bool DrawTextAlignment(EditorContext& ctx, Alignment& alignment) {
	if constexpr (!ReflectedMembers<Alignment>) {
		return DrawValue(ctx, "Alignment", alignment);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(alignment) };

		auto draw_member = [&](auto&& member) {
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized.contains("horizontal")) {
				changed |= DrawValue(ctx, "Horizontal Align", member.value);
			} else if (normalized.contains("vertical")) {
				changed |= DrawValue(ctx, "Vertical Align", member.value);
			} else {
				changed |= DrawValue(ctx, PrettyName(member.name), member.value);
			}
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

		return changed;
	}
}

template <typename Shrink>
bool DrawTextShrinkScale(EditorContext& ctx, Shrink& shrink) {
	if constexpr (!ReflectedMembers<Shrink>) {
		return DrawValue(ctx, "Shrink Scale", shrink);
	} else {
		float* minimum{ nullptr };
		float* maximum{ nullptr };
		auto members{ ReflectMembers(shrink) };

		auto find_bound = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;

			if constexpr (std::same_as<Member, float>) {
				const std::string normalized{ NormalizeFeatureName(member.name) };

				if (normalized == "min") {
					minimum = std::addressof(member.value);
				} else if (normalized == "max") {
					maximum = std::addressof(member.value);
				}
			}
		};

		std::apply([&](auto&&... member) { (find_bound(member), ...); }, members);

		if (!minimum || !maximum) {
			return DrawDefaultContents(ctx, shrink);
		}

		bool changed{ false };

		const float normalized_minimum{ std::max(0.0f, *minimum) };
		const float normalized_maximum{ std::max(normalized_minimum, *maximum) };

		if (*minimum != normalized_minimum || *maximum != normalized_maximum) {
			*minimum = normalized_minimum;
			*maximum = normalized_maximum;
			changed	 = true;
		}

		if (DrawValue(
				ctx, "Min Scale", *minimum,
				FieldOptions{
					.speed	= 0.01f,
					.format = "%.3f",
				}
			)) {
			*minimum = std::clamp(*minimum, 0.0f, *maximum);
			changed	 = true;
		}

		if (DrawValue(
				ctx, "Max Scale", *maximum,
				FieldOptions{
					.speed	= 0.01f,
					.format = "%.3f",
				}
			)) {
			*maximum = std::max(std::max(0.0f, *maximum), *minimum);
			changed	 = true;
		}

		return changed;
	}
}

template <typename Style>
bool DrawTextBoxAdditionalStyle(EditorContext& ctx, Style& style) {
	if constexpr (!ReflectedMembers<Style>) {
		return DrawDefaultContents(ctx, style);
	} else {
		bool changed{ false };
		bool direct_wrap_settings_drawn{ false };
		auto members{ ReflectMembers(style) };

		auto draw_member = [&](auto&& member) {
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized == "alignment") {
				changed |= DrawTextAlignment(ctx, member.value);
				return;
			}
			if (normalized == "horizontalalign") {
				changed |= DrawValue(ctx, "Horizontal Align", member.value);
				return;
			}
			if (normalized == "verticalalign") {
				changed |= DrawValue(ctx, "Vertical Align", member.value);
				return;
			}
			if (normalized == "wrap") {
				changed |= DrawTextWrapMode(ctx, member.value, true);
				changed |= DrawTextWrapSettings(ctx, member.value, true);
				return;
			}
			if (normalized == "wrapmode") {
				changed |= DrawValue(ctx, "Wrap Mode", member.value);
				return;
			}
			if (normalized == "collapsespaces") {
				changed |= DrawValue(ctx, "Collapse Spaces", member.value);
				return;
			}
			if (normalized == "justifylastline") {
				changed |= DrawValue(ctx, "Justify Last Line", member.value);
				return;
			}
			if (IsTextWrapSettingName(normalized)) {
				if (!direct_wrap_settings_drawn) {
					changed					   |= DrawTextWrapSettings(ctx, style, false);
					direct_wrap_settings_drawn	= true;
				}
				return;
			}
			if (normalized == "shrinkscale") {
				const bool open{ ImGui::TreeNodeEx(
					"Shrink to Fit##TextShrinkToFit", ImGuiTreeNodeFlags_SpanAvailWidth
				) };
				if (open) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
					changed |= DrawTextShrinkScale(ctx, member.value);
					ImGui::TreePop();
				}
				return;
			}

			changed |= DrawValue(ctx, PrettyName(member.name), member.value);
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);
		return changed;
	}
}

template <std::size_t I, typename Target, typename TextData>
bool DrawTextBoxMember(Target& target, TextData& text_data, auto& box) {
	using Box = std::remove_cvref_t<decltype(box)>;
	bool changed{ false };
	auto& editor_state{ GetManualFeatureState(target.GetFeatureTargetKey()) };

	bool box_has_non_default_data{ false };

	if constexpr (JsonSerializable<Box> && std::default_initializable<Box>) {
		json current			 = box;
		json defaults			 = Box{};
		box_has_non_default_data = current != defaults;
	}

	if (!editor_state.text_box_state_initialized) {
		editor_state.text_box_enabled			= box_has_non_default_data;
		editor_state.text_box_state_initialized = true;
	} else if (box_has_non_default_data) {
		editor_state.text_box_enabled = true;
	}

	bool enabled{ editor_state.text_box_enabled };

	ScopedID box_scope{ "TextBox" };

	if (ImGui::Checkbox("##Enabled", &enabled)) {
		editor_state.text_box_enabled = enabled;

		if (!enabled) {
			box = Box{};
		}

		changed = true;
	}

	ImGui::SameLine();

	const bool open{ ImGui::TreeNodeEx("Text Box##Tree", ImGuiTreeNodeFlags_SpanAvailWidth) };

	if (!open) {
		return changed;
	}

	ScopedIndent indent;
	ScopedPropertyLabelOffset box_label_offset{ ImGui::GetStyle().IndentSpacing };
	ScopedDisabled disabled{ !enabled };
	auto box_members{ ReflectMembers(box) };

	auto draw_box_member = [&](auto&& box_member) {
		const std::string box_name{ NormalizeFeatureName(box_member.name) };

		if (box_name == "rect") {
			using BoxMember = std::remove_cvref_t<decltype(box_member.value)>;

			if constexpr (std::same_as<BoxMember, Rect>) {
				auto box_locator = [](TextData& value) -> Box& {
					auto reflected{ ReflectMembers(value) };
					return std::get<I>(reflected).value;
				};

				auto rect_locator = [box_locator](TextData& value) -> Rect& {
					auto reflected{ ReflectMembers(box_locator(value)) };
					constexpr std::size_t count{ std::tuple_size_v<decltype(reflected)> };
					Rect* result{ nullptr };

					[&]<std::size_t... Index>(std::index_sequence<Index...>) {
						(
							[&] {
								auto& candidate{ std::get<Index>(reflected) };

								if constexpr (
									std::same_as<
										std::remove_cvref_t<decltype(candidate.value)>, Rect>
								) {
									if (NormalizeFeatureName(candidate.name) == "rect") {
										result = std::addressof(candidate.value);
									}
								}
							}(),
							...);
					}(std::make_index_sequence<count>{});

					return *result;
				};

				changed |= DrawGeometryValue(
					target, text_data, box_member.value, "Rect", rect_locator, &MarkTextLayoutDirty
				);
			} else {
				changed |= DrawValue(target.ctx, "Rect", box_member.value);
			}

			return;
		}

		if (box_name == "style") {
			const bool style_open{ ImGui::TreeNodeEx(
				"Additional Options##TextBoxAdditionalOptions", ImGuiTreeNodeFlags_SpanAvailWidth
			) };

			if (style_open) {
				changed |= DrawTextBoxAdditionalStyle(target.ctx, box_member.value);

				ImGui::TreePop();
			}

			return;
		}

		changed |= DrawValue(target.ctx, PrettyName(box_member.name), box_member.value);
	};

	std::apply([&](auto&&... box_member) { (draw_box_member(box_member), ...); }, box_members);

	ImGui::TreePop();
	return changed;
}

template <std::size_t I, typename Target, typename TextData>
bool DrawTextPrimaryMember(Target& target, TextData& text_data) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if (normalized == "text" || normalized == "content" || normalized == "defaults" ||
		normalized.contains("richtextsource")) {
		return false;
	}

	if (normalized.contains("glyph") || normalized.contains("clip") ||
		normalized.contains("currentrun")) {
		return false;
	}

	if (normalized == "box") {
		using Box = std::remove_cvref_t<decltype(member.value)>;

		if constexpr (ReflectedMembers<Box>) {
			return DrawTextBoxMember<I>(target, text_data, member.value);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <typename Target, typename TextData, std::size_t... I>
bool DrawTextPrimaryMembers(Target& target, TextData& text_data, std::index_sequence<I...>) {
	bool changed{ false };
	((changed |= DrawTextPrimaryMember<I>(target, text_data)), ...);
	return changed;
}

template <typename Target>
bool DrawTextPrimary(Target& target, ::ptgn::impl::TextData& text_data) {
	bool changed{ false };

	// TextData stores resolved StyledText only. Rich-text Defaults are not encoded in
	// the first run, so always use the default-constructed authoring baseline.
	TextRunDefaults defaults{};

	std::string source{ SerializeStyledTextToRichText(text_data.text, defaults) };
	if (DrawRichTextEditor(target.ctx, source, defaults)) {
		text_data.text = ParseRichText(source, defaults).text;
		text_data.current_run_index =
			text_data.text.runs.empty() ? 0 : text_data.text.runs.size() - 1;
		changed = true;
	}

	auto members{ ReflectMembers(text_data) };
	changed |= DrawTextPrimaryMembers(
		target, text_data, std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
	);
	return changed;
}

template <std::size_t OuterIndex, std::size_t InnerIndex, typename Target, typename Clip>
bool DrawTextClipMember(Target& target, ::ptgn::impl::TextData& text_data, Clip& clip) {
	auto members{ ReflectMembers(clip) };
	auto& member{ std::get<InnerIndex>(members) };
	using Member = std::remove_cvref_t<decltype(member.value)>;
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if constexpr (std::same_as<Member, Rect>) {
		if (normalized.contains("rect")) {
			auto locator = [](auto& root) -> Rect& {
				auto outer{ ReflectMembers(root) };
				auto& clip_value{ std::get<OuterIndex>(outer).value };

				if constexpr (kIsOptional<std::remove_cvref_t<decltype(clip_value)>>) {
					auto inner{ ReflectMembers(*clip_value) };
					return std::get<InnerIndex>(inner).value;
				} else {
					auto inner{ ReflectMembers(clip_value) };
					return std::get<InnerIndex>(inner).value;
				}
			};

			return DrawGeometryValue(
				target, text_data, member.value, "Clip Rect", locator, &MarkTextLayoutDirty
			);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <std::size_t OuterIndex, typename Target, typename Clip, std::size_t... InnerIndex>
bool DrawTextClipMembers(
	Target& target, ::ptgn::impl::TextData& text_data, Clip& clip,
	std::index_sequence<InnerIndex...>
) {
	bool changed{ false };
	((changed |= DrawTextClipMember<OuterIndex, InnerIndex>(target, text_data, clip)), ...);
	return changed;
}

template <std::size_t I, typename Target>
bool DrawTextAdditionalMember(Target& target, ::ptgn::impl::TextData& text_data) {
	auto members{ ReflectMembers(text_data) };
	auto& member{ std::get<I>(members) };
	const std::string normalized{ NormalizeFeatureName(member.name) };

	if (!normalized.contains("glyph") && !normalized.contains("clip")) {
		return false;
	}

	using Member = std::remove_cvref_t<decltype(member.value)>;

	if (normalized.contains("clip")) {
		if constexpr (std::same_as<Member, Rect>) {
			ImGui::SeparatorText("Clip Rect");

			auto locator = [](auto& root) -> Rect& {
				auto reflected{ ReflectMembers(root) };
				return std::get<I>(reflected).value;
			};

			return DrawGeometryValue(
				target, text_data, member.value, "Clip Rect", locator, &MarkTextLayoutDirty
			);
		} else if constexpr (kIsOptional<Member>) {
			using OptionalValue = typename Member::value_type;

			if constexpr (std::same_as<OptionalValue, Rect>) {
				bool changed{ false };
				bool enabled{ member.value.has_value() };

				ImGui::PushID(member.name.data(), member.name.data() + member.name.size());

				if (DrawOptionalLabelRow("Clip Rect", enabled, false)) {
					if (enabled) {
						member.value.emplace();
					} else {
						member.value.reset();
					}
					changed = true;
				}

				if (member.value) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };

					auto locator = [](auto& root) -> Rect& {
						auto reflected{ ReflectMembers(root) };
						return *std::get<I>(reflected).value;
					};

					changed |= DrawGeometryValue(
						target, text_data, *member.value, "Clip Rect", locator, &MarkTextLayoutDirty
					);
				}

				ImGui::PopID();
				return changed;
			} else if constexpr (ReflectedMembers<OptionalValue>) {
				bool enabled{ member.value.has_value() };
				bool changed{ false };

				ImGui::PushID(member.name.data(), member.name.data() + member.name.size());

				changed |= DrawOptionalLabelRow(PrettyName(member.name), enabled, false);

				if (enabled != member.value.has_value()) {
					if (enabled) {
						member.value.emplace();
					} else {
						member.value.reset();
					}
					changed = true;
				}

				if (member.value) {
					ScopedIndent indent;
					ScopedPropertyLabelOffset label_offset{ ImGui::GetStyle().IndentSpacing };
					auto clip_members{ ReflectMembers(*member.value) };

					changed |= DrawTextClipMembers<I>(
						target, text_data, *member.value,
						std::make_index_sequence<std::tuple_size_v<decltype(clip_members)>>{}
					);
				}

				ImGui::PopID();
				return changed;
			} else {
				return DrawValue(target.ctx, PrettyName(member.name), member.value);
			}
		} else if constexpr (ReflectedMembers<Member>) {
			auto clip_members{ ReflectMembers(member.value) };

			return DrawTextClipMembers<I>(
				target, text_data, member.value,
				std::make_index_sequence<std::tuple_size_v<decltype(clip_members)>>{}
			);
		}
	}

	return DrawValue(target.ctx, PrettyName(member.name), member.value);
}

template <typename Target, std::size_t... I>
bool DrawTextAdditionalMembers(
	Target& target, ::ptgn::impl::TextData& text_data, std::index_sequence<I...>
) {
	bool changed{ false };
	((changed |= DrawTextAdditionalMember<I>(target, text_data)), ...);
	return changed;
}

template <typename Target>
bool DrawTextAdditional(Target& target, ::ptgn::impl::TextData& text_data) {
	auto members{ ReflectMembers(text_data) };

	return DrawTextAdditionalMembers(
		target, text_data, std::make_index_sequence<std::tuple_size_v<decltype(members)>>{}
	);
}

template <typename T>
bool DrawFlattenedConfig(EditorContext& ctx, T& value) {
	if constexpr (!ReflectedMembers<T>) {
		return DrawDefaultContents(ctx, value);
	} else {
		bool changed{ false };
		auto members{ ReflectMembers(value) };

		auto draw_member = [&](auto&& member) {
			ScopedID member_scope{ static_cast<const void*>(std::addressof(member.value)) };
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized == "config" || normalized == "data") {
				if constexpr (
					ReflectedValue<Member> || ReflectedMembers<Member> ||
					ReflectedReadOnlyMembers<Member>
				) {
					changed |= DrawDefaultContents(ctx, member.value);
					return;
				}
			}

			changed |= DrawValue(ctx, PrettyName(member.name), member.value);
		};

		std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

		return changed;
	}
}

template <typename Target>
bool DrawLineWidthVisual(Target& target) {
	if constexpr (!Target::template Supports<FillStyle>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<FillStyle>()) };

		auto before{ target.template Capture<FillStyle>() };
		bool enabled{ before.has_value() };
		bool changed{ false };

		if (ImGui::Checkbox("##Enabled", &enabled)) {
			target.template SetLive<FillStyle>(
				enabled ? ComponentState<FillStyle>{ FillStyle{ kInspectorMinLineWidth } }
						: std::nullopt
			);
			changed = true;
		}

		ImGui::SameLine();

		const float checkbox_offset{ ImGui::GetFrameHeight() + ImGui::GetStyle().ItemSpacing.x };
		ScopedPropertyLabelOffset label_offset{ checkbox_offset };

		float line_width{ kInspectorMinLineWidth };
		if (const auto current{ target.template Capture<FillStyle>() }) {
			line_width = current->GetLineWidth().value_or(kInspectorMinLineWidth);
		}

		{
			ScopedDisabled disabled{ !enabled };
			if (DrawValue(
					target.ctx, "Line Width", line_width,
					FieldOptions{
						.speed	= kInspectorScalarDragSpeed,
						.min	= kInspectorMinLineWidth,
						.max	= 1000.0f,
						.format = "%.2f",
						.flags	= ImGuiSliderFlags_AlwaysClamp,
					}
				)) {
				target.template SetLive<FillStyle>(FillStyle{ line_width });
				changed = true;
			}
		}

		auto after{ target.template Capture<FillStyle>() };
		TrackComponentState(
			target, "Edit Line Width", std::move(before), std::move(after), changed
		);

		return changed;
	}
}

template <typename T>
float GetShapeRadiusLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, float>) {
		return std::abs(value);
	} else if constexpr (std::same_as<Value, V2_float>) {
		return std::max(std::abs(value.x), std::abs(value.y));
	} else if constexpr (kIsArray<Value> || kIsVector<Value>) {
		float limit{ 0.0f };
		for (const auto& element : value) {
			limit = std::max(limit, GetShapeRadiusLimit(element));
		}
		return limit;
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeRadiusLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if (normalized.contains("radius") || normalized.contains("radii")) {
				if constexpr (
					std::same_as<Member, float> || std::same_as<Member, V2_float> ||
					kIsArray<Member> || kIsVector<Member> || ReflectedValue<Member> ||
					ReflectedMembers<Member>
				) {
					limit = std::max(limit, GetShapeRadiusLimit(member.value));
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetShapeRadiusLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect_member(member), ...); }, members);

		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetShapeSizeLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, Rect>) {
		const V2_float size{ value.GetSize() };
		return std::max(std::abs(size.x), std::abs(size.y));
	} else if constexpr (ReflectedValue<Value>) {
		return GetShapeSizeLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect_member = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, Rect>) {
				limit = std::max(limit, GetShapeSizeLimit(member.value));
			} else if constexpr (std::same_as<Member, V2_float>) {
				if (normalized.contains("size") || normalized.contains("dimension")) {
					limit = std::max(
						limit, std::max(std::abs(member.value.x), std::abs(member.value.y))
					);
				}
			} else if constexpr (std::same_as<Member, float>) {
				if (normalized == "width" || normalized == "height") {
					limit = std::max(limit, std::abs(member.value));
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetShapeSizeLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect_member(member), ...); }, members);

		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetEllipseLineWidthLimit(const T& value) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (std::same_as<Value, V2_float>) {
		return std::min(std::abs(value.x), std::abs(value.y));
	} else if constexpr (ReflectedValue<Value>) {
		return GetEllipseLineWidthLimit(ReflectValue(const_cast<Value&>(value)).value);
	} else if constexpr (ReflectedMembers<Value>) {
		float limit{ 0.0f };
		std::optional<float> x_radius;
		std::optional<float> y_radius;
		auto members{ ReflectMembers(const_cast<Value&>(value)) };

		auto inspect = [&](auto&& member) {
			using Member = std::remove_cvref_t<decltype(member.value)>;
			const std::string normalized{ NormalizeFeatureName(member.name) };

			if constexpr (std::same_as<Member, V2_float>) {
				if (normalized.contains("radius") || normalized.contains("radii")) {
					limit = std::max(
						limit, std::min(std::abs(member.value.x), std::abs(member.value.y))
					);
				}
			} else if constexpr (std::same_as<Member, float>) {
				if (normalized == "radiusx" || normalized == "xradius") {
					x_radius = std::abs(member.value);
				} else if (normalized == "radiusy" || normalized == "yradius") {
					y_radius = std::abs(member.value);
				}
			} else if constexpr (ReflectedValue<Member> || ReflectedMembers<Member>) {
				limit = std::max(limit, GetEllipseLineWidthLimit(member.value));
			}
		};

		std::apply([&](auto&&... member) { (inspect(member), ...); }, members);
		if (x_radius && y_radius) {
			limit = std::max(limit, std::min(*x_radius, *y_radius));
		}
		return limit;
	} else {
		return 0.0f;
	}
}

template <typename T>
float GetShapeLineWidthLimit(const T& value, const Transform& transform) {
	using Value = std::remove_cvref_t<T>;

	float limit{ 1000.0f };

	if constexpr (std::same_as<Value, Rect>) {
		const V2_float size{ value.GetSize(transform) };
		limit = std::min(size.x, size.y) * 0.5f;
	} else if constexpr (std::same_as<Value, RoundedRect>) {
		const V2_float size{ value.rect.GetSize(transform) };
		const float half_min_size{ std::min(size.x, size.y) * 0.5f };
		limit = half_min_size;
	} else if constexpr (std::same_as<Value, Ellipse>) {
		const V2_float radius{ value.GetRadius(transform) };
		limit = std::min(radius.x, radius.y);
	} else if constexpr (
		std::same_as<Value, Circle> || std::same_as<Value, Capsule> || std::same_as<Value, Arc>
	) {
		limit = value.GetRadius(transform);
	}

	return std::max(kInspectorMinLineWidth, limit);
}

template <typename Target>
Transform GetShapeLineWidthTransform(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<Transform>()) {
			return GetDrawTransform(entity);
		}
	}

	if constexpr (Target::template Supports<Transform>()) {
		return target.template Capture<Transform>().value_or(Transform{});
	}

	return {};
}

template <typename Target, typename Shape>
bool DrawShapeFillStyle(Target& target) {
	if constexpr (!Target::template Supports<FillStyle>()) {
		return false;
	} else {
		const Shape shape{ target.template Capture<Shape>().value_or(Shape{}) };
		const Transform transform{ GetShapeLineWidthTransform(target) };

		const float unscaled_line_width_limit{ GetShapeLineWidthLimit(shape, Transform{}) };
		const float scaled_line_width_limit{ GetShapeLineWidthLimit(shape, transform) };
		const float line_width_scale{ scaled_line_width_limit / unscaled_line_width_limit };

		return DrawOptionalComponent<Target, FillStyle>(
			target, "Style", false,
			[&target, unscaled_line_width_limit, scaled_line_width_limit,
			 line_width_scale](FillStyle& style) {
				FillStyle displayed_style{ style };

				if (const auto stored_line_width{ style.GetLineWidth() }) {
					displayed_style = FillStyle{ std::max(
						kInspectorMinLineWidth, stored_line_width.value() / line_width_scale
					) };
				}

				if (!DrawFillStyle(
						target.ctx, "Style", displayed_style, unscaled_line_width_limit
					)) {
					return false;
				}

				if (const auto displayed_line_width{ displayed_style.GetLineWidth() }) {
					style = FillStyle{ std::clamp(
						displayed_line_width.value() * line_width_scale, kInspectorMinLineWidth,
						scaled_line_width_limit
					) };
				} else {
					style = FillStyle{ Solid{} };
				}

				return true;
			}
		);
	}
}

template <typename Target>
bool DrawShapeVisual(Target& target, std::string_view visual) {
	bool changed{ false };

	auto draw_shape = [&]<typename T>() {
		changed |= DrawRequiredInlineVisualComponentWithDefault<Target, T>(
			target, TypeLabel<T>(), MakeDefaultShapeGeometry<T>(),
			[&target](T& value) { return DrawGeometryComponent(target, value); }
		);
	};

	if (visual == "rect") {
		draw_shape.template operator()<Rect>();
	} else if (visual == "circle") {
		draw_shape.template operator()<Circle>();
	} else if (visual == "roundedrect") {
		draw_shape.template operator()<RoundedRect>();
	} else if (visual == "polygon") {
		draw_shape.template operator()<Polygon>();
	} else if (visual == "ellipse") {
		draw_shape.template operator()<Ellipse>();
	} else if (visual == "triangle") {
		draw_shape.template operator()<Triangle>();
	} else if (visual == "line") {
		draw_shape.template operator()<Line>();
	} else if (visual == "capsule") {
		draw_shape.template operator()<Capsule>();
	} else if (visual == "arc") {
		draw_shape.template operator()<Arc>();
	}

	changed |= DrawOptionalVisualComponent<Target, Color>(target, "Color");
	if (visual == "line") {
		changed |= DrawLineWidthVisual(target);
	} else if (visual == "rect") {
		changed |= DrawShapeFillStyle<Target, Rect>(target);
	} else if (visual == "circle") {
		changed |= DrawShapeFillStyle<Target, Circle>(target);
	} else if (visual == "roundedrect") {
		changed |= DrawShapeFillStyle<Target, RoundedRect>(target);
	} else if (visual == "capsule") {
		changed |= DrawShapeFillStyle<Target, Capsule>(target);
	} else if (visual == "ellipse") {
		changed |= DrawShapeFillStyle<Target, Ellipse>(target);
	} else if (visual == "arc") {
		changed |= DrawShapeFillStyle<Target, Arc>(target);
	} else {
		changed |= DrawOptionalVisualComponent<Target, FillStyle>(target, "Style");
	}

	return changed;
}

bool DrawAnimationDataFlattened(
	EditorContext& ctx, ::ptgn::impl::AnimationData& animation,
	std::optional<std::size_t> detected_frame_count, std::optional<V2_int> texture_size
) {
	const auto initial_frame_size{ animation.config.frame_size };
	const V2_int initial_start_pixel{ animation.config.start_pixel };

	bool changed{ false };
	auto members{ ReflectMembers(animation) };

	auto draw_member = [&](auto&& member) {
		const std::string normalized{ NormalizeFeatureName(member.name) };

		using Member = std::remove_cvref_t<decltype(member.value)>;

		if (normalized == "config") {
			if constexpr (ReflectedMembers<Member>) {
				auto config_members{ ReflectMembers(member.value) };

				std::apply(
					[&](auto&&... config_member) {
						auto draw_config_member = [&](auto&& mem) {
							const std::string normalized_name{ NormalizeFeatureName(mem.name) };

							using Value = std::remove_cvref_t<decltype(mem.value)>;

							if constexpr (std::same_as<Value, std::size_t>) {
								if (normalized_name == "framecount") {
									if (detected_frame_count) {
										mem.value = *detected_frame_count;
									}

									{
										ScopedDisabled disabled{ detected_frame_count.has_value() };

										changed |= DrawValue(ctx, PrettyName(mem.name), mem.value);
									}

									if (detected_frame_count) {
										DrawTooltip(
											"Frame count is automatically detected from the "
											"texture asset file name."
										);
									}

									return;
								}
							}

							changed |= DrawValue(ctx, PrettyName(mem.name), mem.value);
						};

						(draw_config_member(config_member), ...);
					},
					config_members
				);

				return;
			}
		}

		changed |= DrawValue(ctx, PrettyName(member.name), member.value);
	};

	std::apply([&](auto&&... member) { (draw_member(member), ...); }, members);

	{
		auto& start_pixel{ animation.config.start_pixel };
		const V2_int before_start_pixel{ start_pixel };
		const auto before_frame_size{ animation.config.frame_size };
		const std::size_t before_current_frame{ animation.current_frame };
		const bool frame_size_edited{ animation.config.frame_size != initial_frame_size };
		const bool start_pixel_edited{ start_pixel != initial_start_pixel };

		start_pixel.x = std::max(start_pixel.x, 0);
		start_pixel.y = std::max(start_pixel.y, 0);

		if (animation.config.frame_size.has_value()) {
			auto& frame_size{ *animation.config.frame_size };
			frame_size.x = std::max(frame_size.x, 1);
			frame_size.y = std::max(frame_size.y, 1);
		}

		std::size_t available_frame_count{ animation.config.frame_count };

		if (texture_size.has_value() && texture_size->IsPositive()) {
			const V2_int size{ *texture_size };
			start_pixel.x = std::min(start_pixel.x, size.x - 1);
			start_pixel.y = std::min(start_pixel.y, size.y - 1);

			if (animation.config.frame_size.has_value()) {
				auto& frame_size{ *animation.config.frame_size };
				frame_size.x = std::min(frame_size.x, size.x);
				frame_size.y = std::min(frame_size.y, size.y);

				if (frame_size_edited && !start_pixel_edited) {
					frame_size.x = std::min(frame_size.x, size.x - start_pixel.x);
					frame_size.y = std::min(frame_size.y, size.y - start_pixel.y);
				} else {
					start_pixel.x = std::min(start_pixel.x, size.x - frame_size.x);
					start_pixel.y = std::min(start_pixel.y, size.y - frame_size.y);
				}

				frame_size.x  = std::min(frame_size.x, size.x - start_pixel.x);
				frame_size.y  = std::min(frame_size.y, size.y - start_pixel.y);
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size.x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size.y);

				available_frame_count =
					static_cast<std::size_t>((size.x - start_pixel.x) / frame_size.x);
			} else if (
				const auto frame_size{
					::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count) };
				frame_size && frame_size->IsPositive()
			) {
				start_pixel.x = std::min(start_pixel.x, size.x - frame_size->x);
				start_pixel.y = std::min(start_pixel.y, size.y - frame_size->y);
				available_frame_count =
					static_cast<std::size_t>((size.x - start_pixel.x) / frame_size->x);
			} else {
				available_frame_count = 0;
			}
		}

		const std::size_t usable_frame_count{
			std::min(animation.config.frame_count, available_frame_count)
		};

		animation.current_frame =
			usable_frame_count == 0 ? 0 : std::min(animation.current_frame, usable_frame_count - 1);

		changed |= start_pixel != before_start_pixel;
		changed |= animation.config.frame_size != before_frame_size;
		changed |= animation.current_frame != before_current_frame;
	}

	if (ctx.local.settings.show_read_only_inspector_data) {
		[&]<typename T>(T& value) {
			if constexpr (ReflectedReadOnlyMembers<T>) {
				auto read_only_members{ ReflectReadOnlyMembers(value) };

				std::apply(
					[&](auto&&... member) {
						(DrawReadOnlyValue(ctx, PrettyName(member.name), member.value), ...);
					},
					read_only_members
				);
			}
		}(animation);
	}

	return changed;
}

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value);

template <typename Value>
[[nodiscard]] std::optional<V2_int> ExtractTexturePixelSize(const Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return value;
	} else if constexpr (std::same_as<Type, V2_float>) {
		return V2_int{ static_cast<int>(std::round(value.x)),
					   static_cast<int>(std::round(value.y)) };
	} else if constexpr (ReflectedValue<Type>) {
		return ExtractTexturePixelSize(ReflectValue(value).value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return ExtractTexturePixelSize(std::get<0>(members).value);
		}
	}

	return std::nullopt;
}

template <typename Value>
bool DrawTextureSizeAsIntegers(EditorContext& ctx, Value& value) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_int>) {
		return DrawWHValue("Texture Size", value, 1.0f, 0, 4096);
	} else if constexpr (std::same_as<Type, V2_float>) {
		V2_int displayed{ static_cast<int>(std::round(value.x)),
						  static_cast<int>(std::round(value.y)) };

		if (!DrawWHValue("Texture Size", displayed, 1.0f, 0, 4096)) {
			return false;
		}

		value = V2_float{ static_cast<float>(displayed.x), static_cast<float>(displayed.y) };
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return DrawTextureSizeAsIntegers(ctx, member.value);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return DrawTextureSizeAsIntegers(ctx, std::get<0>(members).value);
		} else {
			return DrawDefaultContents(ctx, value);
		}
	} else {
		return DrawDefaultContents(ctx, value);
	}
}

template <typename Value>
bool SetTextureSizeFromPixels(Value& value, V2_float pixels) {
	using Type = std::remove_cvref_t<Value>;

	if constexpr (std::same_as<Type, V2_float>) {
		value = pixels;
		return true;
	} else if constexpr (std::same_as<Type, V2_int>) {
		value = V2_int{ static_cast<int>(std::round(pixels.x)),
						static_cast<int>(std::round(pixels.y)) };
		return true;
	} else if constexpr (ReflectedValue<Type>) {
		auto member{ ReflectValue(value) };
		return SetTextureSizeFromPixels(member.value, pixels);
	} else if constexpr (ReflectedMembers<Type>) {
		auto members{ ReflectMembers(value) };

		if constexpr (std::tuple_size_v<decltype(members)> == 1) {
			return SetTextureSizeFromPixels(std::get<0>(members).value, pixels);
		}
	}

	return false;
}

template <typename Target>
[[nodiscard]] std::optional<std::size_t> ResolveDetectedAnimationFrameCount(const Target& target) {
	if constexpr (!Target::template Supports<TextureKey>()) {
		return std::nullopt;
	} else {
		const auto texture_key{ target.template Capture<TextureKey>() };

		if (!texture_key) {
			return std::nullopt;
		}

		return ::ptgn::impl::DetectAnimationFrameCount(
			target.ctx.editor.GetAssetManager(), *texture_key
		);
	}
}

template <typename Target>
[[nodiscard]] std::optional<V2_int> ResolveAnimationTextureSize(const Target& target) {
	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity) {
			const auto texture_size{ GetTextureSize(entity) };
			return texture_size && texture_size->IsPositive() ? texture_size : std::nullopt;
		}
	}

	if constexpr (Target::template Supports<::ptgn::impl::TextureSize>()) {
		if (const auto texture_size{ target.template Capture<::ptgn::impl::TextureSize>() }) {
			const auto pixels{ ExtractTexturePixelSize(*texture_size) };
			return pixels && pixels->IsPositive() ? pixels : std::nullopt;
		}
	}

	return std::nullopt;
}

template <typename Target>
bool SynchronizeAnimationFrameData(Target& target, std::string_view reason) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop	= ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<AnimationData>()) {
		return false;
	} else {
		auto before_animation{ target.template Capture<AnimationData>() };

		if (!before_animation) {
			return false;
		}

		AnimationData animation{ *before_animation };

		if (const auto detected{ ResolveDetectedAnimationFrameCount(target) }) {
			animation.config.frame_count = *detected;
		}

		const auto texture_size{ ResolveAnimationTextureSize(target) };

		if (!animation.config.frame_size.has_value()) {
			animation.config.frame_size =
				::ptgn::impl::GetFrameSize(texture_size, animation.config.frame_count);
		}

		if (animation.config.frame_count == 0) {
			animation.current_frame = 0;
		} else {
			animation.current_frame %= animation.config.frame_count;
		}

		animation.frame_dirty = true;
		target.template SetLive<AnimationData>(animation);
		auto after_animation{ target.template Capture<AnimationData>() };
		TrackComponentState(target, reason, std::move(before_animation), after_animation, true);

		if constexpr (Target::template Supports<TextureCrop>()) {
			auto before_crop{ target.template Capture<TextureCrop>() };
			TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
			crop.Update(animation, texture_size);
			target.template SetLive<TextureCrop>(crop);
			auto after_crop{ target.template Capture<TextureCrop>() };
			TrackComponentState(
				target, "Update Animation Texture Crop", std::move(before_crop),
				std::move(after_crop), true
			);
		}

		return true;
	}
}

template <typename Target>
bool SynchronizeAnimationTextureCrop(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;
	using TextureCrop	= ::ptgn::impl::TextureCrop;

	if constexpr (
		!Target::template Supports<AnimationData>() || !Target::template Supports<TextureCrop>()
	) {
		return false;
	} else {
		const auto animation{ target.template Capture<AnimationData>() };

		if (!animation) {
			return false;
		}

		auto before_crop{ target.template Capture<TextureCrop>() };
		TextureCrop crop{ before_crop.value_or(TextureCrop{}) };
		crop.Update(*animation, ResolveAnimationTextureSize(target));
		target.template SetLive<TextureCrop>(crop);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target, "Update Animation Texture Crop", std::move(before_crop), std::move(after_crop),
			true
		);
		return true;
	}
}

template <typename Target>
bool DisableAnimationTextureCrop(Target& target) {
	using TextureCrop = ::ptgn::impl::TextureCrop;

	if constexpr (!Target::template Supports<TextureCrop>()) {
		return false;
	} else {
		auto before_crop{ target.template Capture<TextureCrop>() };

		if (!before_crop) {
			return false;
		}

		target.template SetLive<TextureCrop>(std::nullopt);
		auto after_crop{ target.template Capture<TextureCrop>() };
		TrackComponentState(
			target, "Disable Texture Crop", std::move(before_crop), std::move(after_crop), true
		);
		return true;
	}
}

bool DrawSpriteStackData(
	EditorContext& ctx, SpriteStackData& data, std::optional<int> detected_slice_count
) {
	bool changed{ false };

	int displayed_slice_count{ detected_slice_count.value_or(data.slice_count) };

	{
		ScopedDisabled disabled{ detected_slice_count.has_value() };

		changed |= DrawValue(
			ctx, "Slice Count", displayed_slice_count,
			FieldOptions{
				.speed	= 1.0f,
				.min	= 1.0f,
				.max	= 10000.0f,
				.format = "%d",
				.flags	= ImGuiSliderFlags_AlwaysClamp,
			}
		);
	}

	if (detected_slice_count) {
		DrawTooltip(
			"Slice count is automatically detected from the "
			"_slicesN suffix in the texture key."
		);
	} else if (changed) {
		data.slice_count = static_cast<std::size_t>(std::max(displayed_slice_count, 1));
	}

	changed |= DrawValue(ctx, "Slice Order", data.slice_order);

	changed |= DrawValue(
		ctx, "Layer Offset", data.layer_offset,
		FieldOptions{
			.speed	= 0.1f,
			.min	= -1000.0f,
			.max	= 1000.0f,
			.format = "%.2f",
		}
	);

	changed |= DrawValue(ctx, "Pixel Snap", data.pixel_snap);

	return changed;
}

template <typename Target, AssetKeyType Key>
bool DrawOptionalAssetComponent(Target& target, std::string_view label);

template <typename Target>
bool DrawSpriteStackPrimary(Target& target) {
	bool changed{ false };

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	const auto texture_key{ target.template Capture<TextureKey>() };

	const std::optional<std::size_t> detected_slice_count{
		texture_key ? ::ptgn::impl::DetectSpriteStackSliceCount(
						  target.ctx.editor.GetAssetManager(), *texture_key
					  )
					: std::nullopt
	};

	changed |= DrawRequiredInlineVisualComponent<Target, SpriteStackData>(
		target, "Sprite Stack", [&target, detected_slice_count](SpriteStackData& value) {
			return DrawSpriteStackData(target.ctx, value, detected_slice_count);
		}
	);

	return changed;
}

template <typename Target>
bool DrawSpritePrimaryImpl(Target& target) {
	using AnimationData = ::ptgn::impl::AnimationData;

	bool changed{ false };
	const auto before_texture{ target.template Capture<TextureKey>() };
	const auto before_animation{ target.template Capture<AnimationData>() };
	const auto before_texture_size{ target.template Capture<::ptgn::impl::TextureSize>() };

	std::optional<V2_float> texture_size_default;

	if (!before_texture_size && before_animation && before_animation->config.frame_size) {
		V2_float scale{ 1.0f };

		if constexpr (Target::template Supports<Transform>()) {
			if (const auto transform{ target.template Capture<Transform>() }) {
				scale = V2_float{ std::abs(transform->scale.x), std::abs(transform->scale.y) };
			}
		}

		const auto frame_size{ *before_animation->config.frame_size };

		texture_size_default = V2_float{ static_cast<float>(frame_size.x) * scale.x,
										 static_cast<float>(frame_size.y) * scale.y };
	}

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	const auto detected_frame_count{ ResolveDetectedAnimationFrameCount(target) };

	changed |= DrawOptionalComponent<Target, ::ptgn::impl::TextureSize>(
		target, "Texture Size", false,
		[&target, before_texture_size, texture_size_default](::ptgn::impl::TextureSize& value) {
			bool initialized{ false };

			if (!before_texture_size && texture_size_default) {
				initialized = SetTextureSizeFromPixels(value, *texture_size_default);
			}

			return DrawTextureSizeAsIntegers(target.ctx, value) || initialized;
		}
	);

	const auto animation_texture_size{ ResolveAnimationTextureSize(target) };

	changed |= DrawOptionalComponent<Target, AnimationData>(
		target, "Animation", true,
		[&target, detected_frame_count, animation_texture_size](AnimationData& value) {
			const std::size_t before_frame_count{ value.config.frame_count };

			const bool local_changed{ DrawAnimationDataFlattened(
				target.ctx, value, detected_frame_count, animation_texture_size
			) };

			const bool frame_count_changed{ before_frame_count != value.config.frame_count };

			if (frame_count_changed) {
				if (!value.config.frame_size.has_value()) {
					value.config.frame_size = ::ptgn::impl::GetFrameSize(
						animation_texture_size, value.config.frame_count
					);
				}

				value.current_frame = value.config.frame_count == 0
										? 0
										: value.current_frame % value.config.frame_count;

				value.frame_dirty = true;
			}

			return local_changed || frame_count_changed;
		}
	);

	const auto after_texture{ target.template Capture<TextureKey>() };
	const auto after_animation{ target.template Capture<AnimationData>() };
	const bool texture_changed{ before_texture != after_texture };
	const bool animation_enabled{ after_animation.has_value() };
	const bool animation_was_enabled{ before_animation.has_value() };
	const bool frame_count_changed{ before_animation && after_animation &&
									before_animation->config.frame_count !=
										after_animation->config.frame_count };

	if (!animation_enabled && animation_was_enabled) {
		changed |= DisableAnimationTextureCrop(target);
	} else if (animation_enabled && texture_changed) {
		changed |=
			SynchronizeAnimationFrameData(target, "Recalculate Animation Frame Size From Texture");
	} else if (animation_enabled && (!animation_was_enabled || frame_count_changed)) {
		changed |= SynchronizeAnimationTextureCrop(target);
	}

	return changed;
}

template <typename Target>
bool DrawMaterialDetails(Target& target, ::ptgn::Material& material) {
	bool changed{ false };

	changed |= DrawVectorEditor(
		target.ctx, material.uniforms,
		VectorOptions{
			.item_name = "Uniform",
			.add_label = "+ Add Uniform",
			.add_first = true,
		}
	);

	const std::size_t max_texture_slots{
		std::max(std::size_t{ 1 }, static_cast<std::size_t>(target.ctx.editor.GetMaxTextureSlots()))
	};

	changed |= DrawValue(
		target.ctx, "Texture Slot Capacity", material.texture_slot_capacity,
		FieldOptions{
			.speed	= 1.0f,
			.min	= 1.0f,
			.max	= static_cast<float>(max_texture_slots),
			.format = "%llu",
			.flags	= ImGuiSliderFlags_AlwaysClamp,
		}
	);

	if (material.texture_slot_capacity.has_value()) {
		const std::size_t clamped{
			std::clamp(*material.texture_slot_capacity, std::size_t{ 1 }, max_texture_slots)
		};

		if (*material.texture_slot_capacity != clamped) {
			material.texture_slot_capacity = clamped;
			changed						   = true;
		}
	}

	return changed;
}

template <typename Target, AssetKeyType Key>
bool DrawOptionalAssetComponent(Target& target, std::string_view label) {
	if constexpr (!Target::template Supports<Key>()) {
		return false;
	} else {
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Key>()) };

		auto before{ target.template Capture<Key>() };
		std::optional<Key> value{ before };

		const bool changed{ DrawOptionalAssetKeyInline(target.ctx, label, value, FieldOptions{}) };

		if (changed) {
			target.template SetLive<Key>(value);
		}

		auto after{ target.template Capture<Key>() };

		TrackComponentState(
			target, std::string{ "Edit " } + std::string{ label }, std::move(before),
			std::move(after), changed
		);

		return changed;
	}
}

template <typename Target>
bool DrawCustomShaderMaterial(Target& target, ::ptgn::Material& material) {
	std::optional<ShaderKey> shader{ material.shader.value.empty()
										 ? std::nullopt
										 : std::optional<ShaderKey>{ material.shader } };

	bool changed{ DrawOptionalAssetKeyInline(target.ctx, "Shader Key", shader, FieldOptions{}) };

	if (changed) {
		material.shader = shader.value_or(ShaderKey{});
	}

	changed |= DrawMaterialDetails(target, material);
	return changed;
}

template <typename Target>
bool DrawCustomShaderPrimary(Target& target) {
	bool changed{ false };

	if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
		auto before{ target.template Capture<::ptgn::Material>() };
		bool material_changed{ false };

		if (!before) {
			target.template SetLive<::ptgn::Material>(::ptgn::Material{});
			material_changed = true;
		}

		{
			ScopedID target_scope{ target.Id() };
			ScopedID component_scope{ static_cast<int>(Hash<::ptgn::Material>()) };
			auto& material{ target.entity.template Get<::ptgn::Material>() };
			material_changed |= DrawCustomShaderMaterial(target, material);
		}

		auto after{ target.template Capture<::ptgn::Material>() };
		TrackComponentState(
			target, "Edit Material", std::move(before), std::move(after), material_changed
		);

		changed |= material_changed;
	} else {
		changed |= DrawRequiredComponent<Target, ::ptgn::Material>(
			target, "Material", false, [&target](::ptgn::Material& material) {
				return DrawCustomShaderMaterial(target, material);
			}
		);
	}

	changed |= DrawOptionalAssetComponent<Target, TextureKey>(target, "Texture Key");

	changed |= DrawOptionalComponent<Target, Rect>(
		target, "Size", false,
		[](Rect& value) {
			V2_float size{ value.GetSize() };

			if (!DrawWHValue("Size", size, kInspectorSizeDragSpeed, 0.0f, 0.0f, "%.3f")) {
				return false;
			}

			size.x = std::max(0.0f, size.x);
			size.y = std::max(0.0f, size.y);

			const V2_float center{ value.GetCenter() };
			const V2_float half_size{ size * 0.5f };

			value.min = center - half_size;
			value.max = center + half_size;
			return true;
		},
		false, false, nullptr, Rect{ V2_float{ 100.0f, 100.0f } }
	);

	return changed;
}

template <typename Target>
bool DrawSpriteAdditional(Target& target) {
	bool changed{ false };
	const bool animated{ target.template Capture<::ptgn::impl::AnimationData>().has_value() };

	changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::Offsets>(target, "Offsets", true);

	if (animated) {
		changed |= DrawReadOnlyExistingReflected<Target, ::ptgn::impl::TextureCrop>(
			target, "Texture Crop", true
		);
	} else {
		changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::TextureCrop>(
			target, "Texture Crop", true
		);
	}

	return changed;
}

template <typename Target, typename T>
bool DrawOptionalNamedValue(Target& target, std::string_view label) {
	return DrawOptionalComponent<Target, T>(target, label, false, [&target, label](T& value) {
		return [&target, label]<typename Value>(Value& reflected_value) {
			if constexpr (ReflectedValue<Value>) {
				auto member{ ReflectValue(reflected_value) };
				return DrawValue(target.ctx, label, member.value);
			} else if constexpr (ReflectedMembers<Value>) {
				auto members{ ReflectMembers(reflected_value) };

				if constexpr (std::tuple_size_v<decltype(members)> == 1) {
					auto& member{ std::get<0>(members) };
					return DrawValue(target.ctx, label, member.value);
				} else {
					return DrawMembers(target.ctx, reflected_value);
				}
			} else {
				return DrawDefaultContents(target.ctx, reflected_value);
			}
		}(value);
	});
}

template <typename Target>
bool DrawRenderTargetPrimary(Target& target) {
	bool changed{ false };

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	std::optional<V2_int> framebuffer_size;

	if constexpr (requires { target.entity; }) {
		Entity entity{ target.entity };

		if (entity && entity.Has<::ptgn::impl::FramebufferObject>()) {
			const V2_int actual_size{ RenderTarget{ entity }.GetSize() };

			if (actual_size.IsPositive()) {
				framebuffer_size = actual_size;
			}
		}
	}

	if (primary_scene_target) {
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::RenderTargetDesc>(
			target, "Render Target",
			[&target, framebuffer_size](::ptgn::impl::RenderTargetDesc& value) {
				return DrawRenderTargetDesc(target.ctx, value, framebuffer_size, true);
			}
		);
	} else {
		changed |= DrawOptionalComponent<Target, ::ptgn::impl::RenderTargetDesc>(
			target, "Render Target", false,
			[&target, framebuffer_size](::ptgn::impl::RenderTargetDesc& value) {
				return DrawRenderTargetDesc(target.ctx, value, framebuffer_size);
			}
		);
	}

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearColor>(target, "Clear Color");

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearDepth>(target, "Clear Depth");

	changed |= DrawOptionalNamedValue<Target, ::ptgn::impl::ClearStencil>(target, "Clear Stencil");

	return changed;
}

template <typename Target>
bool DrawVisualLayers(Target& target) {
	if constexpr (
		!Target::template Supports<::ptgn::impl::RenderMask>() ||
		!Target::template Supports<::ptgn::impl::UILayer>()
	) {
		return false;
	} else {
		auto before_mask{ target.template Capture<::ptgn::impl::RenderMask>() };
		auto before_ui{ target.template Capture<::ptgn::impl::UILayer>() };

		::ptgn::impl::RenderMask mask{ before_mask.value_or(::ptgn::impl::RenderMask{}) };
		bool ui_layer{ before_ui.has_value() };

		ScopedID target_scope{ target.Id() };
		ScopedID layers_scope{ "VisualLayers" };

		const bool changed{ DrawLayerMaskValue("Layers", mask.layers, ui_layer) };

		if (!changed) {
			return false;
		}

		target.template SetLive<::ptgn::impl::RenderMask>(mask);

		ComponentState<::ptgn::impl::UILayer> ui_state;
		if (ui_layer) {
			ui_state = ::ptgn::impl::UILayer{};
		}
		target.template SetLive<::ptgn::impl::UILayer>(ui_state);

		auto after_mask{ target.template Capture<::ptgn::impl::RenderMask>() };
		auto after_ui{ target.template Capture<::ptgn::impl::UILayer>() };

		auto apply_mask{ target.template MakeApply<::ptgn::impl::RenderMask>() };
		auto apply_ui{ target.template MakeApply<::ptgn::impl::UILayer>() };
		const ImGuiID key{ ImGui::GetID("##VisualLayersEdit") };

		TrackUndoableInteraction(
			target.ctx, key, "Edit Layers", true,
			[apply_mask, apply_ui, before_mask, before_ui]() mutable {
				apply_mask(before_mask);
				apply_ui(before_ui);
			},
			[apply_mask, apply_ui, after_mask, after_ui]() mutable {
				apply_mask(after_mask);
				apply_ui(after_ui);
			}
		);

		return true;
	}
}

template <typename Target>
bool DrawVisualAdditionalOptions(Target& target, std::string_view visual, bool draw_tint) {
	const bool open{
		ImGui::TreeNodeEx("Additional Options##Visual", ImGuiTreeNodeFlags_SpanAvailWidth)
	};

	if (!open) {
		return false;
	}

	bool changed{ false };

	{
		ScopedUnindent align_with_additional_options;

		if (visual.contains("sprite")) {
			changed |= DrawSpriteAdditional(target);
		}

		if (visual == "rect" || visual == "circle" || visual == "sprite") {
			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::ShadowCaster>(
				target, "Shadow Caster", true
			);
		}

		if (visual.contains("text")) {
			ScopedID text_additional_scope{ "TextAdditional" };
			changed |= DrawRequiredComponent<Target, ::ptgn::impl::TextData>(
				target, "Text Additional Options", false,
				[&target](::ptgn::impl::TextData& value) {
					return DrawTextAdditional(target, value);
				},
				&MarkTextLayoutDirty
			);
		}

		changed |= DrawOptionalVisualComponent<Target, BlendMode>(target, "Blend Mode");

		if (draw_tint) {
			changed |= DrawOptionalVisualComponent<Target, Tint>(target, "Tint");

			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::IgnoreParentTint>(
				target, "Ignore Parent Tint"
			);
		}

		if (!IsPrimarySceneRenderTarget(target)) {
			changed |= DrawOptionalVisualComponent<Target, ::ptgn::impl::IgnoreParentVisibility>(
				target, "Ignore Parent Visibility"
			);

			changed |= DrawVisualLayers(target);
		}
	}

	ImGui::TreePop();
	return changed;
}

template <typename Target, typename Visuals, typename Draw, typename Callback>
bool DrawButtonChildStateVisualComponent(
	Target& target, ButtonVisualState state, std::string_view part_label, Draw&& draw,
	Callback callback, bool draw_visual_separator = true
) {
	if constexpr (!Target::template Supports<Visuals>()) {
		return false;
	} else {
		if (draw_visual_separator) {
			ImGui::SeparatorText("Visual");
		}
		AutoLabelWidthScope label_width{ "ButtonVisualStateVisualFields" };
		ScopedID target_scope{ target.Id() };
		ScopedID component_scope{ static_cast<int>(Hash<Visuals>()) };

		auto before{ target.template Capture<Visuals>() };
		Visuals visuals{ before.value_or(Visuals{}) };
		const auto index{ static_cast<std::size_t>(std::to_underlying(state)) };
		auto& visual{ visuals.states[index] };

		const bool changed{ std::invoke(std::forward<Draw>(draw), visuals, visual) };

		if (changed) {
			// The part checkbox owns whether this visual state is defined. Editing or clearing an
			// individual optional property must never disable the entire part.
			visual.defined = true;
			target.template SetLive<Visuals>(ComponentState<Visuals>{ visuals }, callback);
		}

		auto after{ target.template Capture<Visuals>() };
		TrackComponentState(
			target, std::string{ "Edit " } + std::string{ part_label } + " State Visual",
			std::move(before), std::move(after), changed, callback
		);

		return changed;
	}
}


template <typename Target>
bool DrawButtonChildStateVisualFeatureImpl(
	Target& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	switch (child_info.part) {
		case ButtonChildPart::Background:
			return DrawButtonChildStateVisualComponent<Target, ButtonBackgroundVisuals>(
				target, state, "Button Background",
				[&target, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					DrawTooltip("Local origin used by this part.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					return changed;
				},
				&MarkButtonBackgroundDirty
			);

		case ButtonChildPart::Border:
			return DrawButtonChildStateVisualComponent<Target, ButtonBorderVisuals>(
				target, state, "Button Border",
				[&target, &child_info, state](auto& visuals, ButtonShapeVisual&) {
					bool changed{ false };
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Size", visuals.states, state, &ButtonShapeVisual::size
					);
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonShapeVisual::origin
					);
					DrawTooltip("Local origin used by this part.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonShapeVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");
					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Color", visuals.states, state, &ButtonShapeVisual::color
					);
					changed |= DrawButtonBorderLineWidth(child_info.button, visuals.states, state);
					return changed;
				},
				&MarkButtonBorderDirty
			);

		case ButtonChildPart::Text:
			return DrawButtonChildStateVisualComponent<Target, ButtonTextVisuals>(
				target, state, "Button Text",
				[&target, state](ButtonTextVisuals& visuals, ButtonTextVisual& visual) {
					bool changed{ false };

					// Draw the managed text using the same TextData component drawer used by ordinary
					// Text entities. Changes to content/text-box data are copied back into the active
					// button visual state, while TextData-only properties stay on the managed entity.
					auto text_before{ target.template Capture<::ptgn::impl::TextData>() };
					::ptgn::impl::TextData text_data{
						text_before.value_or(::ptgn::impl::TextData{})
					};
					const ::ptgn::impl::TextData displayed_before{ text_data };
					if (DrawInspectorValueContents(
						target.ctx, Hash<::ptgn::impl::TextData>(), std::addressof(text_data)
					)) {
						if (text_data.text != displayed_before.text) {
						visual.styled_text = text_data.text;
					}
						if (text_data.box != displayed_before.box) {
							visual.box = text_data.box;
						}
						visual.defined = true;
						target.template SetLive<::ptgn::impl::TextData>(
							text_data, &MarkTextLayoutDirty
						);
						changed = true;
					}

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonTextVisual::origin
					);
					DrawTooltip("Local origin used by this part.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonTextVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");

					changed |= DrawButtonVisualTriStateBoolOverrideValue(
						target.ctx, "Auto Box", visuals.states, state, &ButtonTextVisual::auto_box
					);
					DrawTooltip("Inherit, enable, or disable automatic text-box sizing.");

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Padding", visuals.states, state, &ButtonTextVisual::padding
					);

					return changed;
				},
				&MarkButtonTextDirty, false
			);

		case ButtonChildPart::Sprite:
			return DrawButtonChildStateVisualComponent<Target, ButtonSpriteVisuals>(
				target, state, "Button Sprite",
				[&target, state](auto& visuals, ButtonSpriteVisual&) {
					bool changed{ false };

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Key", visuals.states, state,
						&ButtonSpriteVisual::texture
					);

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Origin", visuals.states, state, &ButtonSpriteVisual::origin
					);
					DrawTooltip("Local origin used by this part.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Anchor", visuals.states, state, &ButtonSpriteVisual::anchor
					);
					DrawTooltip("Point on the button this part is anchored to.");

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Texture Size", visuals.states, state, &ButtonSpriteVisual::size
					);

					changed |= DrawButtonVisualOverrideValue(
						target.ctx, "Tint", visuals.states, state, &ButtonSpriteVisual::tint
					);

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Animation", visuals.states, state,
						&ButtonSpriteVisual::animation, [&target](AnimationConfig& animation) {
							return DrawInspectorValueContents(
								target.ctx, Hash<AnimationConfig>(), std::addressof(animation)
							);
						}
					);

					changed |= DrawButtonVisualOverrideTree(
						target.ctx, "Animation Options", visuals.states, state,
						&ButtonSpriteVisual::animation_options
					);

					return changed;
				},
				&MarkButtonSpriteDirty
			);
	}

	return false;
}

template <typename Target>
bool DrawVisualFeatureImpl(Target& target, bool draw_header = true) {
	if (const auto child_info{ GetButtonChildInfo(target) }) {
		if (const auto state{ GetButtonVisualEditState(target) }) {
			return DrawButtonChildStateVisualFeatureImpl(target, *child_info, *state);
		}
	}

	if (!HasVisualFeature(target)) {
		return false;
	}

	const bool primary_scene_target{ IsPrimarySceneRenderTarget(target) };

	FeatureHeaderResult header{
		.open	 = true,
		.changed = false,
	};

	if (draw_header) {
		header = DrawFeatureHeader(
			target, InspectorFeature::Visual, "Visual", ImGuiTreeNodeFlags_DefaultOpen,
			VisualFeatureComponents{}, !primary_scene_target
		);

		if (!header.open) {
			return header.changed;
		}
	} else {
		ImGui::SeparatorText("Visual");
	}

	std::optional<ScopedIndent> feature_indent;
	if (draw_header) {
		feature_indent.emplace();
	}
	AutoLabelWidthScope visual_label_width{ "VisualFeatureFields" };

	bool changed{ header.changed };
	const RendererRowResult renderer{ DrawRendererRow(target) };
	const std::string& visual{ renderer.visual };
	changed |= renderer.changed;

	if (renderer.changed) {
		return true;
	}

	if (visual.empty()) {
		ImGui::TextDisabled("Choose a renderer to expose its relevant components.");
		return changed;
	}

	changed |= DrawVisualEffects(target, visual);

	bool draw_tint{ false };
	const bool shape{ visual == "rect" || visual == "circle" || visual == "roundedrect" ||
					  visual == "polygon" || visual == "ellipse" || visual == "triangle" ||
					  visual == "line" || visual == "capsule" || visual == "arc" };

	if (shape) {
		changed |= DrawShapeVisual(target, visual);
	} else if (visual == "spritestack") {
		changed	  |= DrawSpriteStackPrimary(target);
		draw_tint  = true;
	} else if (visual.contains("sprite")) {
		changed	  |= DrawSpritePrimaryImpl(target);
		draw_tint  = true;
	} else if (visual.contains("text")) {
		ScopedID text_primary_scope{ "TextPrimary" };
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::TextData>(
			target, "Text",
			[&target](::ptgn::impl::TextData& value) { return DrawTextPrimary(target, value); },
			&MarkTextLayoutDirty
		);
		draw_tint = true;
	} else if (visual.contains("particle")) {
		changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::ParticleEmitterData>(
			target, "Particle Emitter", [&target](::ptgn::impl::ParticleEmitterData& value) {
				return DrawFlattenedConfig(target.ctx, value);
			}
		);
	} else if (visual.contains("light")) {
		changed |= DrawRequiredInlineVisualComponent<Target, LightData>(
			target, "Light", [&target](LightData& value) {
				return DrawRegisteredComponentContents(
					target.ctx, Hash<LightData>(), std::addressof(value)
				);
			}
		);
	} else if (visual.contains("customshader")) {
		changed |= DrawCustomShaderPrimary(target);
	} else if (visual.contains("graphics")) {
		if constexpr (std::same_as<std::remove_cvref_t<Target>, EntityInspectorTarget>) {
			auto before{ target.template Capture<::ptgn::impl::GraphicsData>() };
			bool graphics_changed{ false };

			if (!before) {
				target.template SetLive<::ptgn::impl::GraphicsData>(::ptgn::impl::GraphicsData{});
				graphics_changed = true;
			}

			{
				ScopedID target_scope{ target.Id() };
				ScopedID component_scope{ static_cast<int>(Hash<::ptgn::impl::GraphicsData>()) };
				auto& value{ target.entity.template Get<::ptgn::impl::GraphicsData>() };
				graphics_changed |= DrawRegisteredComponentContents(
					target.ctx, Hash<::ptgn::impl::GraphicsData>(), std::addressof(value)
				);
			}

			auto after{ target.template Capture<::ptgn::impl::GraphicsData>() };
			TrackComponentState(
				target, "Edit Graphics", std::move(before), std::move(after), graphics_changed
			);
			changed |= graphics_changed;
		} else {
			changed |= DrawRequiredInlineVisualComponent<Target, ::ptgn::impl::GraphicsData>(
				target, "Graphics", [&target](::ptgn::impl::GraphicsData& value) {
					return DrawRegisteredComponentContents(
						target.ctx, Hash<::ptgn::impl::GraphicsData>(), std::addressof(value)
					);
				}
			);
		}
	} else if (visual.contains("rendertarget")) {
		changed |= DrawRenderTargetPrimary(target);
	}

	if (primary_scene_target) {
		const Origin origin{ target.template Capture<Origin>().value_or(Origin::Center) };

		DrawReadOnlyValue(target.ctx, "Origin", origin);
	} else {
		changed |= DrawOptionalVisualComponent<Target, Origin>(target, "Origin");
	}
	changed |= DrawVisualAdditionalOptions(target, visual, draw_tint);

	return changed;
}


} // namespace

bool DrawVisualFeature(EntityInspectorTarget& target, bool draw_header) {
	return DrawVisualFeatureImpl(target, draw_header);
}

bool DrawVisualFeature(PrefabInspectorTarget& target, bool draw_header) {
	return DrawVisualFeatureImpl(target, draw_header);
}

bool DrawButtonChildStateVisualFeature(
	EntityInspectorTarget& target, const ButtonChildInfo& child_info, ButtonVisualState state
) {
	return DrawButtonChildStateVisualFeatureImpl(target, child_info, state);
}

bool DrawSpritePrimary(EntityInspectorTarget& target) {
	return DrawSpritePrimaryImpl(target);
}

} // namespace ptgn::editor::inspector
