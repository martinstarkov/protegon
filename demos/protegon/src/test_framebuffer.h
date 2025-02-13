#pragma once

#include "common.h"

#include "renderer/gl_renderer.h"
#include "core/sdl_instance.h"
#include "SDL.h"
#include "renderer/gl_helper.h"
#include "renderer/gl_loader.h"

void TestFrameBuffer() {
    game.Init();
    FrameBuffer::Unbind();
    game.window.SetSize({ 640, 480 });
	game.window.SetSetting(WindowSetting::Shown);

    // Set viewport
    gl::glViewport(0, 0, 640, 480);

    // Render loop
    while (true) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                return;
            }
        }

        // Clear the screen
        gl::glClearColor(1.0f, 0.0f, 1.0f, 1.0f);
        gl::glClear(GL_COLOR_BUFFER_BIT);

        // Swap buffers
        game.window.SwapBuffers();
    }

    game.Stop();
}