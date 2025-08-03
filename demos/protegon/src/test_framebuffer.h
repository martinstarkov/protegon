#pragma once

#include "SDL.h"
#include "common.h"
#include "core/sdl_instance.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"
#include "renderer/gl_renderer.h"


void TestFrameBuffer() {
	game.Init();
	FrameBuffer::Unbind();
	game.window.SetSize({ 640, 480 });
	game.window.SetSetting(WindowSetting::Shown);

	// Set viewport
	glViewport(0, 0, 640, 480);

	// Render loop
	while (true) {
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				return;
			}
		}

		// Clear the screen
		glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		// Swap buffers
		game.window.SwapBuffers();
	}

	game.Stop();
}