#include <functional>
#include <iostream>
#include <ostream>
#include <utility>
#include <vector>

#include "app/application.h"
#include "core/event/dispatcher.h"
#include "core/event/event.h"
#include "core/math/geometry/rect.h"
#include "core/math/transform.h"
#include "core/math/vector2.h"
#include "core/util/hash.h"
#include "core/util/type_info.h"
#include "renderer/primitives/color.h"
#include "renderer/renderer.h"
#include "runtime/ecs/entity.h"
#include "runtime/ecs/game_object.h"
#include "runtime/graphics/shape.h"
#include "runtime/scene/scene.h"
#include "runtime/scene/scene_input.h"
#include "runtime/scripting/script.h"
#include "runtime/scripting/scripts.h"
#include "runtime/ui/interactive.h"

struct TestScene : public Scene {
	Entity CreateInteractiveRect(V2_float size) {
		auto entity = CreateEntity();
		entity.Add<Rect>(size);
		return entity;
	}

	void OnEnter() override {
		V2_float rsize{ 100, 50 };

		auto r		= CreateRect(*this, {}, rsize, color::Green, 1.0f);
		auto rchild = CreateInteractiveRect(rsize);
		AddInteractiveShape(r, GameObject{ std::move(rchild) });

		struct Normal {};

		struct Hovered {};

		struct Pressed {};

		AddFSM(r)
			.Initial<Normal>()

			.Transition<Normal, MouseEnter, Hovered>()
			.Action([](Entity e) { std::cout << "Hover start\n"; })

			.Transition<Hovered, MouseLeave, Normal>()
			.Action([](Entity e) { std::cout << "Hover end\n"; })

			.Transition<Hovered, MousePressedOver, Pressed>()
			.Action([](Entity e) { std::cout << "Pressed\n"; })

			.Transition<Pressed, MouseReleasedOver, Hovered>()
			.Action([](Entity e) { std::cout << "Click\n"; });

		AddFSM(r)
			.Initial<Normal>()

			.Transition<Normal, MouseEnter, Hovered>()
			.Action([](Entity e) { std::cout << "Hover animation (infinite loop)\n"; })

			.Transition<Hovered, MouseLeave, Normal>()

			.Transition<Hovered, MousePressedOver, Pressed>()
			.Action([](Entity e) { std::cout << "Pressed animation (infinite loop)\n"; })

			.Transition<Pressed, MouseReleasedOver, Hovered>()
			.Action([](Entity e) { std::cout << "Click animation (once)\n"; });
	}
};

int main(int, char**) {
	Application app{ "TestScene" };
	app.StartWith<TestScene>();
}

/*
#include <SDL3/SDL.h>
#include <SDL3/SDL_opengl.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <stdio.h>
#include <string.h>

#define WINDOW_W   800
#define WINDOW_H   600
#define WRAP_WIDTH 300

GLuint texture_from_surface(SDL_Surface* surf) {
	SDL_Surface* converted = SDL_ConvertSurface(surf, SDL_PIXELFORMAT_RGBA32);

	GLuint tex;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);

	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
		converted->pixels
	);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	SDL_DestroySurface(converted);

	return tex;
}

int main(int argc, char* argv[]) {
	SDL_Init(SDL_INIT_VIDEO);
	TTF_Init();

	SDL_Window* window =
		SDL_CreateWindow("SDL3_ttf OpenGL Test", WINDOW_W, WINDOW_H, SDL_WINDOW_OPENGL);

	SDL_GLContext glctx = SDL_GL_CreateContext(window);

	glViewport(0, 0, WINDOW_W, WINDOW_H);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, WINDOW_W, WINDOW_H, 0, -1, 1);

	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();

	glEnable(GL_TEXTURE_2D);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	TTF_Font* font = TTF_OpenFont("assets/Arial.ttf", 24);
	if (!font) {
		printf("Font load error: %s\n", SDL_GetError());
		return 1;
	}

	char text[4096]	   = "Wrapping test: ";
	const char* source = "The quick brown fox jumps over the lazy dog. ";

	int source_index = 0;
	Uint64 last_add	 = SDL_GetTicks();

	SDL_Color white = { 255, 255, 255, 255 };

	int running = 1;

	while (running) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_EVENT_QUIT) {
				running = 0;
			}
		}

		Uint64 now = SDL_GetTicks();

		size_t len = strlen(text);

		if (now - last_add > 50) {
			text[len]	  = source[source_index];
			text[len + 1] = '\0';

			source_index++;
			if (source[source_index] == '\0') {
				source_index = 0;
			}

			last_add = now;
		}

		glClearColor(0.08f, 0.08f, 0.08f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);

		SDL_Surface* surf = TTF_RenderText_Blended_Wrapped(font, text, len, white, WRAP_WIDTH);

		if (surf) {
			GLuint tex = texture_from_surface(surf);

			float x = 50.5f;
			float y = 50.5f;
			float w = (float)surf->w;
			float h = (float)surf->h;

			glBindTexture(GL_TEXTURE_2D, tex);

			glBegin(GL_QUADS);

			glTexCoord2f(0.0f, 0.0f);
			glVertex2f(x, y);

			glTexCoord2f(1.0f, 0.0f);
			glVertex2f(x + w, y);

			glTexCoord2f(1.0f, 1.0f);
			glVertex2f(x + w, y + h);

			glTexCoord2f(0.0f, 1.0f);
			glVertex2f(x, y + h);

			glEnd();

			glDeleteTextures(1, &tex);
			SDL_DestroySurface(surf);
		}

		SDL_GL_SwapWindow(window);
	}

	TTF_CloseFont(font);

	SDL_GL_DestroyContext(glctx);
	SDL_DestroyWindow(window);

	TTF_Quit();
	SDL_Quit();

	return 0;
}
*/