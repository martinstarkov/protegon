#pragma once

namespace ptgn {

// Acts as a tag implying the entity is used for player-based systems.
struct PlayerComponent {
	PlayerComponent() = default;
	~PlayerComponent() = default;
};

// Acts as a tag implying the entity is used for input-based systems.
struct InputComponent {
	InputComponent() = default;
	~InputComponent() = default;
};

} // namespace ptgn