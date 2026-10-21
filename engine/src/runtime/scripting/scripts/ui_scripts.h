#pragma once

#include "runtime/scripting/script_common.h"
#include "runtime/ui/dialogue.h"

namespace ptgn {

struct DialogueOpenScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueOpenScript)
};

struct DialogueCloseScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueCloseScript)
};

struct DialogueAdvanceScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueAdvanceScript)
};

struct DialogueNextPageScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueNextPageScript)
};

struct DialogueCompletePageScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueCompletePageScript)
};

struct DialogueChangeDialogueScript : public Script {
	void OnStart() override;
	std::string dialogue{};
	PTGN_REFLECT(DialogueChangeDialogueScript, dialogue)
};

struct DialogueSelectDialogueScript : public Script {
	void OnStart() override;
	std::string dialogue{};
	PTGN_REFLECT(DialogueSelectDialogueScript, dialogue)
};

struct DialogueNextDialogueScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueNextDialogueScript)
};

struct DialogueOpenNextDialogueScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DialogueOpenNextDialogueScript)
};

struct TooltipShowScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipShowScript)
};

struct TooltipHideScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipHideScript)
};

struct TooltipEnableHoverScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipEnableHoverScript)
};

struct TooltipDisableHoverScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipDisableHoverScript)
};

struct TooltipToggleHoverScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipToggleHoverScript)
};

struct TooltipToggleVisibilityScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(TooltipToggleVisibilityScript)
};

struct ButtonPressScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonPressScript)
};

struct ButtonHoverStartScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonHoverStartScript)
};

struct ButtonHoverContinueScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonHoverContinueScript)
};

struct ButtonHoverStopScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonHoverStopScript)
};

struct ButtonEnableScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonEnableScript)
};

struct ButtonDisableScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ButtonDisableScript)
};

struct SliderSetValueScript : public Script {
	void OnStart() override;
	float value{ 0.5f };
	PTGN_REFLECT(SliderSetValueScript, value)
};

struct SliderIncreaseScript : public Script {
	void OnStart() override;
	float value{ 0.5f };
	PTGN_REFLECT(SliderIncreaseScript, value)
};

struct SliderDecreaseScript : public Script {
	void OnStart() override;
	float value{ 0.5f };
	PTGN_REFLECT(SliderDecreaseScript, value)
};

struct ToggleButtonOnScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ToggleButtonOnScript)
};

struct ToggleButtonOffScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ToggleButtonOffScript)
};

struct ToggleButtonToggleScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ToggleButtonToggleScript)
};

struct ToggleGroupSelectScript : public Script {
	void OnStart() override;
	std::string key{};
	PTGN_REFLECT(ToggleGroupSelectScript, key)
};

struct ToggleGroupAllowNoneScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ToggleGroupAllowNoneScript)
};

struct ToggleGroupRequireActiveScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(ToggleGroupRequireActiveScript)
};

struct DropdownOpenScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DropdownOpenScript)
};

struct DropdownCloseScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DropdownCloseScript)
};

struct DropdownToggleScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(DropdownToggleScript)
};

struct DropdownPressItemScript : public Script {
	void OnStart() override;
	float value{ 0.0f };
	PTGN_REFLECT(DropdownPressItemScript, value)
};

struct SetUITextScript : public Script {
	void OnStart() override;
	std::string content{};

	PTGN_REFLECT(SetUITextScript, content)
};

struct TooltipSetPositionScript : public Script {
	void OnStart() override;
	V2_float position{};

	PTGN_REFLECT(TooltipSetPositionScript, position)
};

struct TooltipSetHoverOffsetScript : public Script {
	void OnStart() override;
	V2_float offset{};

	PTGN_REFLECT(TooltipSetHoverOffsetScript, offset)
};

struct DialogueGoToPageScript : public Script {
	void OnStart() override;
	int page_index{};

	PTGN_REFLECT(DialogueGoToPageScript, page_index)
};

struct DialogueSetPortraitScript : public Script {
	void OnStart() override;
	DialoguePortraitSlot slot{ DialoguePortraitSlot::Left };
	std::string speaker{};
	std::string expression{};

	PTGN_REFLECT(DialogueSetPortraitScript, slot, speaker, expression)
};

} // namespace ptgn
