#pragma once

#include "Component.h"

#include "renderer/Color.h"

// TODO: Add color serialization

struct RenderComponent {
	engine::Color color;
	engine::Color original_color;
	RenderComponent(engine::Color color = { 0, 0, 0, 0 }) : color{ color } {
		Init();
	}
	void Init() {
		original_color = color;
	}
	void ResetColor() {
		color = original_color;
	}
};