#pragma once

#include "System.h"

#include "SDL.h"

// TODO: Add key specificity to InputComponent

class InputSystem : public System<InputComponent> {
public:
	virtual void update() override final;
private:
	const Uint8* s = nullptr;
};