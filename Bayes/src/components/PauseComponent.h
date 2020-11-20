#pragma once

struct PauseScreenComponent {
	PauseScreenComponent() = default;
	bool toggleable = true;
	bool open = false;
	int release_time = 0;
};