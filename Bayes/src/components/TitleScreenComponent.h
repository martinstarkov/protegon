#pragma once

struct TitleScreenComponent {
	TitleScreenComponent(bool open = false) : open{ open } {}
	bool open;
};