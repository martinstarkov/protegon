#pragma once

#include "runtime/scripting/script_common.h"

namespace ptgn {

struct DraggableSetFollowMouseScript : public Script {
	void OnStart() override;
	bool option{ true };

	PTGN_REFLECT(DraggableSetFollowMouseScript, option)
};

struct DraggableSetMoveConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DraggableSetMoveConditionScript, text)
};

struct DraggableSetDropConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DraggableSetDropConditionScript, text)
};

struct DraggableSetPickupConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DraggableSetPickupConditionScript, text)
};

struct DropzoneSetMoveConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DropzoneSetMoveConditionScript, text)
};

struct DropzoneSetDropConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DropzoneSetDropConditionScript, text)
};

struct DropzoneSetPickupConditionScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(DropzoneSetPickupConditionScript, text)
};

struct InteractionAddInteractiveShapeScript : public Script {
	void OnStart() override;
	int index{};
	V2_float vector{};
	V2_float vector2{ 32.0f, 32.0f };
	float number{ 12.0f };

	PTGN_REFLECT(InteractionAddInteractiveShapeScript, index, vector, vector2, number)
};

struct InteractionRemoveInteractiveShapeScript : public Script {
	void OnStart() override;
	std::string text{};

	PTGN_REFLECT(InteractionRemoveInteractiveShapeScript, text)
};

struct InteractionClearInteractiveShapesScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(InteractionClearInteractiveShapesScript)
};

struct InteractionLockInteractionsScript : public Script {
	void OnStart() override;
	float number{ 1.0f };
	bool option{ true };

	PTGN_REFLECT(InteractionLockInteractionsScript, number, option)
};

struct InteractionUnlockInteractionsScript : public Script {
	void OnStart() override;
	PTGN_REFLECT_EMPTY(InteractionUnlockInteractionsScript)
};

} // namespace ptgn
