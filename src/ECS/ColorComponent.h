#pragma once
#include "Components.h"
#include "SDL.h"

class ColorComponent : public Component {
public:
	ColorComponent(SDL_Color color = { 0, 0, 0, 255 }) {
		_color = color;
	}
	SDL_Color getColor() { return _color; }
	void setColor(SDL_Color color) {
		_color = color;
	}
private:
	SDL_Color _color;
};