#include "opengl_instance.h"

#include <iostream>
#include <cassert>

#include <SDL.h>
#include <SDL_image.h>

#include "game.h"
#include "renderer/gl_loader.h"
#include "renderer/buffer.h"
#include "protegon/shader.h"

namespace ptgn {

ptgn::OpenGLInstance::OpenGLInstance() {
#ifndef __APPLE__
	initialized_ = InitOpenGL();
	// TODO: Potentially make this optional in the future:
#else
	// TODO: Figure out what to do here for Apple.
	initialized_ = false;
#endif
	assert(initialized_ && "Failed to initialize OpenGL");
}

OpenGLInstance::~OpenGLInstance() {
	// TODO: Figure out if something needs to be done here.
	initialized_ = false;
}

bool OpenGLInstance::IsInitialized() const {
	return initialized_;
}

bool OpenGLInstance::InitOpenGL() {
	#ifndef __APPLE__

	#define STR(x) #x
	#define GLE(name, caps_name) gl##name = (PFNGL##caps_name##PROC) SDL_GL_GetProcAddress(STR(gl##name));
		GL_LIST
	#undef GLE
	#undef STR

	#define GLE(name, caps_name) gl##name && 
			return GL_LIST true;
	#undef GLE

	#else // __APPLE__
		// TODO: Figure out if something needs to be done separately on apple.
		return true;
	#endif
}

namespace impl {

std::shared_ptr<VertexBuffer<float>> vbo[4] = { {}, {}, {}, {} };
GLuint vao[4] = { 0, 0, 0, 0 };
const std::vector<float> vao_vert = {
	-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
	 0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};

//---------------------------------------------------------------------------
static void vao_init() {
	glGenVertexArrays(4, vao);
	glBindVertexArray(vao[0]);

	vbo[0] = VertexBuffer<float>::Create(vao_vert);
	vbo[0]->SetLayout(BufferLayout{
		{ ShaderDataType::vec3 },
		{ ShaderDataType::vec4 },
		});

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
//---------------------------------------------------------------------------
static void vao_exit() {
	glDeleteVertexArrays(4, vao);
	//glDeleteBuffers(4, vbo);
}
//---------------------------------------------------------------------------
static void vao_draw(GLuint mode) {}

static void PresentBuffer(SDL_Renderer* renderer, SDL_Window* win, SDL_Texture* backBuffer, Shader& shader, float playing_time, float w, float h, int rx, int ry, int rw, int rh) {
	//SDL_GL_BindTexture(backBuffer, NULL, NULL);

	SDL_SetRenderTarget(renderer, backBuffer);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	shader.Bind();

	glBindVertexArray(vao[0]);
	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glDrawArrays(GL_TRIANGLES, 0, 3);
	glBindVertexArray(0);

	shader.SetUniform("iResolution", w, h, 0.0f);
	shader.SetUniform("iTime", playing_time);

	shader.Unbind();

	SDL_SetRenderTarget(renderer, NULL);
	SDL_SetTextureBlendMode(backBuffer, SDL_BLENDMODE_BLEND);
	SDL_Rect dest_rect{
		rx, ry,
		rw, rh
	};
	// OpenGL coordinate system is flipped vertically compared to SDL
	SDL_RenderCopyEx(renderer, backBuffer, NULL, &dest_rect, 0, NULL, SDL_FLIP_VERTICAL);
}

void DrawRect(SDL_Renderer* renderer, const SDL_Rect& rect, SDL_Color color) {
	SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
	SDL_RenderFillRect(renderer, &rect);
}

void TestOpenGL() {

	Game& game = global::GetGame();
	OpenGLInstance& opengl = game.opengl;
	assert(opengl.IsInitialized());
	SDLInstance& sdl = game.sdl;
	SDL_Renderer* renderer = sdl.GetRenderer();
	SDL_Window* win = sdl.GetWindow();

	std::string vertex_source = R"(
		#version 330 core

		layout(location = 0) in vec3 pos;
		layout(location = 1) in vec4 color;

		out vec3 v_Position;
		out vec4 v_Color;

		void main() {
			v_Position = pos;
			v_Color = color;
			gl_Position = vec4(pos, 1.0);
		}
	)";

	std::string fragment_source = R"(
		#version 330 core

		layout(location = 0) out vec4 color;

		in vec3 v_Position;
		in vec4 v_Color;

		void main() {
			color = vec4(v_Position * 0.5 + 0.5, 1.0);
			color = v_Color;
		}
	)";

	Shader shader;
	shader.CreateFromStrings(vertex_source, fragment_source);
	//Shader shader = Shader{ "resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl" };

	const float WIN_WIDTH = (float)window::GetSize().x;
	const float WIN_HEIGHT = (float)window::GetSize().y;

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
}



} // namespace impl

} // namespace ptgn