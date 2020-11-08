#pragma once

enum class UIInteractionState {
	ACTIVE,
	HOVER,
	NONE,
};

struct StateComponent {
	StateComponent(UIInteractionState state = UIInteractionState::NONE) : state{ state } {}
	UIInteractionState state;
};