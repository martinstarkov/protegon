#include "runtime/scripting/scripts/ui_scripts.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numbers>
#include <optional>
#include <ranges>
#include <span>
#include <sstream>
#include <string_view>
#include <type_traits>
#include <utility>

#include "app/application_context.h"
#include "app/project.h"
#include "core/graphics/fill_style.h"
#include "core/log.h"
#include "core/math/angle.h"
#include "core/math/geometry/arc.h"
#include "core/math/geometry/capsule.h"
#include "core/math/geometry/circle.h"
#include "core/math/geometry/ellipse.h"
#include "core/math/geometry/line.h"
#include "core/math/geometry/polygon.h"
#include "core/math/geometry/rect.h"
#include "core/math/geometry/rounded_rect.h"
#include "core/math/geometry/triangle.h"
#include "core/math/math_utils.h"
#include "core/math/noise.h"
#include "core/math/rng.h"
#include "core/math/tolerance.h"
#include "runtime/animation/animation.h"
#include "runtime/animation/offsets.h"
#include "runtime/animation/scripted_animation.h"
#include "runtime/audio/audio_system.h"
#include "runtime/ecs/entity_hierarchy.h"
#include "runtime/graphics/draw.h"
#include "runtime/graphics/fx/effect_registry.h"
#include "runtime/graphics/fx/effects.h"
#include "runtime/graphics/fx/light.h"
#include "runtime/graphics/render_target.h"
#include "runtime/graphics/sprite.h"
#include "runtime/graphics/sprite_stack.h"
#include "runtime/graphics/text/text.h"
#include "runtime/graphics/tint.h"
#include "runtime/graphics/visible.h"
#include "runtime/interaction/draggable.h"
#include "runtime/interaction/dropzone.h"
#include "runtime/interaction/interactive.h"
#include "runtime/physics/collider.h"
#include "runtime/physics/lifetime.h"
#include "runtime/physics/movement.h"
#include "runtime/physics/physics.h"
#include "runtime/physics/platformer_jump.h"
#include "runtime/physics/platformer_jump_registry.h"
#include "runtime/physics/rigid_body.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_camera.h"
#include "runtime/scene/scene_context.h"
#include "runtime/scene/scene_file.h"
#include "runtime/scene/scene_manager.h"
#include "runtime/scene/scene_registry.h"
#include "runtime/scene/scene_transitions.h"
#include "runtime/scripting/script_common.h"
#include "runtime/ui/button.h"
#include "runtime/ui/dialogue.h"
#include "runtime/ui/dropdown.h"
#include "runtime/ui/slider.h"
#include "runtime/ui/toggle_button.h"
#include "runtime/ui/tooltip.h"
#include "runtime/world/tilemap.h"

namespace ptgn {

void TooltipShowScript::OnStart() {
	if (target) {
		ShowTooltip(target);
	}
}

void TooltipHideScript::OnStart() {
	if (target) {
		HideTooltip(target);
	}
}

void TooltipEnableHoverScript::OnStart() {
	if (target) {
		SetTooltipOnHoverEnabled(target, true);
	}
}

void TooltipDisableHoverScript::OnStart() {
	if (target) {
		SetTooltipOnHoverEnabled(target, false);
	}
}

void TooltipToggleHoverScript::OnStart() {
	if (target) {
		ToggleTooltipOnHover(target);
	}
}

void TooltipToggleVisibilityScript::OnStart() {
	if (target) {
		ToggleTooltip(target);
	}
}

void DialogueOpenScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.Open();
}

void DialogueCloseScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.Close();
}

void DialogueAdvanceScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.Advance();
}

void DialogueNextPageScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.NextPage();
}

void DialogueCompletePageScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.CompletePage();
}

void DialogueChangeDialogueScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	if (box.Get<impl::DialogueData>().dialogues.contains(dialogue)) {
		box.Open(dialogue);
	}
}

void DialogueSelectDialogueScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	if (box.Get<impl::DialogueData>().dialogues.contains(dialogue)) {
		box.SetDialogue(dialogue);
	}
}

void DialogueNextDialogueScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	box.SetNextDialogue();
}

void DialogueOpenNextDialogueScript::OnStart() {
	if (!target || !target.Has<impl::DialogueData>()) {
		PTGN_WARN("Dialogue action requires a Dialogue target");
		return;
	}
	Dialogue box{ target };
	const auto* current{ box.GetCurrentDialogue() };
	if (current && !current->next_dialogue.empty()) {
		box.Open(current->next_dialogue);
	}
}

void ButtonPressScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.Press();
}

void ButtonHoverStartScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.StartHover();
}

void ButtonHoverContinueScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.ContinueHover();
}

void ButtonHoverStopScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.StopHover();
}

void ButtonEnableScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.Enable(true, true);
}

void ButtonDisableScript::OnStart() {
	if (!target || !target.Has<impl::ButtonData>()) {
		PTGN_WARN("UI action requires Button");
		return;
	}
	Button button{ target };
	button.Disable(true, true);
}

void SliderSetValueScript::OnStart() {
	if (!target || !target.Has<impl::SliderData>()) {
		PTGN_WARN("UI action requires Slider");
		return;
	}
	Slider slider{ target };
	slider.SetValue(value);
}

void SliderIncreaseScript::OnStart() {
	if (!target || !target.Has<impl::SliderData>()) {
		PTGN_WARN("UI action requires Slider");
		return;
	}
	Slider slider{ target };
	slider.SetValue(slider.GetValue() + value);
}

void SliderDecreaseScript::OnStart() {
	if (!target || !target.Has<impl::SliderData>()) {
		PTGN_WARN("UI action requires Slider");
		return;
	}
	Slider slider{ target };
	slider.SetValue(slider.GetValue() - value);
}

void ToggleButtonOnScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonData>()) {
		PTGN_WARN("UI action requires ToggleButton");
		return;
	}
	ToggleButton button{ target };
	button.SetToggled(true);
}

void ToggleButtonOffScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonData>()) {
		PTGN_WARN("UI action requires ToggleButton");
		return;
	}
	ToggleButton button{ target };
	button.SetToggled(false);
}

void ToggleButtonToggleScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonData>()) {
		PTGN_WARN("UI action requires ToggleButton");
		return;
	}
	ToggleButton button{ target };
	button.Press();
}

void ToggleGroupSelectScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonGroupData>()) {
		PTGN_WARN("UI action requires ToggleButtonGroup");
		return;
	}
	ToggleButtonGroup group{ target };
	group.SetActive(key);
}

void ToggleGroupAllowNoneScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonGroupData>()) {
		PTGN_WARN("UI action requires ToggleButtonGroup");
		return;
	}
	ToggleButtonGroup group{ target };
	group.SetAlwaysOneActive(false);
}

void ToggleGroupRequireActiveScript::OnStart() {
	if (!target || !target.Has<impl::ToggleButtonGroupData>()) {
		PTGN_WARN("UI action requires ToggleButtonGroup");
		return;
	}
	ToggleButtonGroup group{ target };
	group.SetAlwaysOneActive(true);
}

void DropdownOpenScript::OnStart() {
	if (!target || !target.Has<impl::DropdownData>()) {
		PTGN_WARN("UI action requires Dropdown");
		return;
	}
	Dropdown dropdown{ target };
	dropdown.Open();
}

void DropdownCloseScript::OnStart() {
	if (!target || !target.Has<impl::DropdownData>()) {
		PTGN_WARN("UI action requires Dropdown");
		return;
	}
	Dropdown dropdown{ target };
	dropdown.Close();
}

void DropdownToggleScript::OnStart() {
	if (!target || !target.Has<impl::DropdownData>()) {
		PTGN_WARN("UI action requires Dropdown");
		return;
	}
	Dropdown dropdown{ target };
	dropdown.Toggle();
}

void DropdownPressItemScript::OnStart() {
	if (!target || !target.Has<impl::DropdownData>()) {
		PTGN_WARN("UI action requires Dropdown");
		return;
	}
	Dropdown dropdown{ target };
	auto items{ dropdown.GetButtons() };
	if (value >= 0.0f && std::isfinite(value) && value < static_cast<float>(items.size())) {
		items[static_cast<std::size_t>(value)].Press();
	}
}

void SetUITextScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::TextData>()) {
		Text{ target }.Content(StyledText{ TextRun{ .text = content } });
	} else if (target.Has<impl::ButtonData>()) {
		Button{ target }.Text().Content(StyledText{ TextRun{ .text = content } });
	} else if (target.Has<impl::TooltipData>() && HasChildren(target)) {
		for (Entity child : GetChildren(target)) {
			if (child.Has<impl::TooltipTextPart>() && child.Has<impl::TextData>()) {
				Text{ child }.Content(StyledText{ TextRun{ .text = content } });
				return;
			}
		}
	}
}

void TooltipSetPositionScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::TooltipData>()) {
		SetPosition(target, position);
	}
}

void TooltipSetHoverOffsetScript::OnStart() {
	if (!target) {
		return;
	}
	if (auto* hover{ target.TryGet<impl::TooltipHoverData>() }) {
		hover->offset = offset;
	}
}

void DialogueGoToPageScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::DialogueData>()) {
		Dialogue{ target }.GoToPage(static_cast<std::size_t>(std::max(0, page_index)));
	}
}

void DialogueSetPortraitScript::OnStart() {
	if (!target) {
		return;
	}
	if (target.Has<impl::DialogueData>()) {
		Dialogue{ target }.SetPortrait(slot, speaker, expression);
	}
}

} // namespace ptgn
