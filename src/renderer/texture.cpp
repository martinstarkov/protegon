#include "protegon/texture.h"

#include <SDL.h>
#include <SDL_image.h>

#include <cassert> // assert

#include "protegon/log.h"
#include "protegon/file.h"
#include "core/game.h"
#include "renderer/gl_loader.h"

namespace ptgn {

Texture::Texture(const char* image_path) {
	assert(*image_path && "Empty image path?");
	assert(FileExists(image_path) && "Nonexistent image path?");
	texture_ = std::shared_ptr<SDL_Texture>(IMG_LoadTexture(global::GetGame().sdl.GetRenderer(), image_path), SDL_DestroyTexture);
	if (!IsValid()) {
		PrintLine(IMG_GetError());
		assert(!"Failed to create texture from image path");
	}
}


Texture::Texture(SDL_Surface* surface) {
	assert(surface != nullptr && "Nullptr surface?");
	texture_ = std::shared_ptr<SDL_Texture>(SDL_CreateTextureFromSurface(global::GetGame().sdl.GetRenderer(), surface), SDL_DestroyTexture);
	if (!IsValid()) {
		PrintLine(SDL_GetError());
		assert(!"Failed to create texture from surface");
	}
	SDL_FreeSurface(surface);
}

bool Texture::IsValid() const {
	return texture_ != nullptr;
}

void Texture::Draw(const Rectangle<float>& texture,
				   const Rectangle<int>& source,
				   float angle,
				   Flip flip,
				   V2_int* center_of_rotation) const {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	assert(renderer != nullptr && "Game uninitialized?");
	assert(IsValid() && "Destroyed or uninitialized texture?");
	SDL_Rect src_rect;
	bool source_given{ !source.size.IsZero() };
	if (source_given) {
		src_rect = { source.pos.x,  source.pos.y,
					 source.size.x, source.size.y };
	}
	SDL_Rect destination{
		static_cast<int>(texture.pos.x),
		static_cast<int>(texture.pos.y),
		static_cast<int>(texture.size.x),
		static_cast<int>(texture.size.y)
	};
	SDL_Point rotation_point;
	if (center_of_rotation != nullptr) {
		rotation_point.x = center_of_rotation->x;
		rotation_point.y = center_of_rotation->y;
	}
	//texture.Draw(color::RED, 1);
	SDL_RenderCopyEx(renderer,
		texture_.get(),
		source_given ? &src_rect : NULL,
		&destination,
		angle,
		center_of_rotation != nullptr ? &rotation_point : NULL,
		static_cast<SDL_RendererFlip>(flip)
	);
}

V2_int Texture::GetSize() const {
	V2_int size;
	SDL_QueryTexture(texture_.get(), NULL, NULL, &size.x, &size.y);
	return size;
}

void Texture::SetAlpha(std::uint8_t alpha) {
	SDL_SetTextureBlendMode(texture_.get(), SDL_BLENDMODE_BLEND);
	SDL_SetTextureAlphaMod(texture_.get(), alpha);
}

void Texture::SetColor(const Color& color) {
	SetAlpha(color.a);
	SDL_SetTextureColorMod(texture_.get(), color.r, color.g, color.b);
}

void DrawRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

void TestFunction() {
	GLuint programId;

	ptgn::impl::InitializeFileSystem();

	if (SDL_Init(SDL_INIT_EVERYTHING | SDL_VIDEO_OPENGL) != 0) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return;
	}

	SDL_Window* win = SDL_CreateWindow("Custom shader with SDL2 renderer!", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, 0);

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

	SDL_Renderer* renderer = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED);

	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(renderer, &rendererInfo);

	if (!strncmp(rendererInfo.name, "opengl", 6)) {
#ifndef __APPLE__
		bool gl_init = ptgn::impl::GLInit();
		assert(gl_init && "Failed to initialize OpenGL extensions");
#endif
		//programId = compileProgram("resources/shader/std.vertex", "resources/shader/crt.fragment");
		//std::cout << "programId = " << programId << std::endl;
	}

	// *shader::Load(0, 

	Shader shader = Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");

	GLfloat iResolution[3] = { WIN_WIDTH, WIN_HEIGHT, 0 };
	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	SDL_Texture* texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);

	int done = 0;
	int useShader = 0;
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	SDL_Texture* drawTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);

	SDL_Rect rect1;
	SDL_Rect rect2;
	SDL_Rect rect3;

	rect1.x = 200;
	rect1.y = 200;

	rect1.w = 60;
	rect1.h = 40;

	rect2.x = 400;
	rect2.y = 400;

	rect2.w = 50;
	rect2.h = 70;

	rect3.x = 0;
	rect3.y = 0;

	rect3.w = 80;
	rect3.h = 80;

	vao_init();

	while (!done) {
		SDL_SetRenderTarget(renderer, NULL);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);

		SDL_Point mouse;

		SDL_GetMouseState(&mouse.x, &mouse.y);

		SDL_Rect dest_rect;

		dest_rect.w = 300;
		dest_rect.h = 200;

		dest_rect.x = mouse.x - dest_rect.w / 2;
		dest_rect.y = mouse.y - dest_rect.h / 2;
		//Render to the texture
		/*SDL_SetRenderTarget(renderer, NULL);
		SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
		SDL_RenderClear(renderer);*/
		//SDL_SetRenderTarget(renderer, texTarget);

		curr_time = clock();
		playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

		/*SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
		SDL_RenderFillRect(renderer, &rect1);

		SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
		SDL_RenderFillRect(renderer, &rect2);

		SDL_SetRenderTarget(renderer, drawTarget);

		SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
		SDL_RenderClear(renderer);

		SDL_SetRenderDrawColor(renderer, 0, 128, 0, 255);
		SDL_RenderFillRect(renderer, &rect3);*/

		//Now render to the texture

		
		//shader.Bind();
		//shader.SetUniform("iResolution", w, h, 0.0f);
		//shader.SetUniform("iTime", playing_time);

		////SDL_GL_SwapWindow(win);

		//shader.Unbind();

		DrawRect(renderer, rect1, { 0, 0, 255, 255 });

		PresentBuffer(renderer, win, drawTarget, shader, playtime_in_second, WIN_WIDTH, WIN_HEIGHT, dest_rect.x, dest_rect.y, dest_rect.w, dest_rect.h);

		DrawRect(renderer, rect2, { 255, 0, 0, 255 });
		// Detach the texture

		//SDL_RenderCopyEx(renderer, texTarget, NULL, NULL, 0, NULL, SDL_FLIP_NONE);

		SDL_RenderPresent(renderer);
		//SDL_SetRenderTarget(renderer, NULL);
		
		/*SDL_RenderPresent(renderer);*/

//		SDL_RenderPresent(renderer);

		/* This could go in a separate function */
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				done = 1;
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_SPACE) {
					useShader ^= 1;
					std::cout << "useShader = " << (useShader ? "true" : "false") << std::endl;
				}
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					done = 1;
				}
			}
		}
	}
	vao_exit();

	SDL_DestroyTexture(texTarget);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}

} // namespace ptgn