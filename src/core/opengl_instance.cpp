#include "opengl_instance.h"

#include <iostream>
#include <cassert>

#include <SDL.h>
#include <SDL_image.h>

#include "game.h"
#include "protegon/window.h"
#include "renderer/gl_loader.h"
#include "renderer/buffer.h"
#include "protegon/shader.h"
#include "renderer/vertex_array.h"

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

const std::vector<std::uint32_t> indices = {
	0, 1, 2,
	2, 3, 1
};

const std::vector<std::uint32_t> indices2 = {
	0, 1, 2,
};

struct Vertex {
	glsl::vec3 pos;
	glsl::vec4 color;
};

const std::vector<Vertex> vao_vert = {
	Vertex{ glsl::vec3{ -1.0f, -1.0f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{  1.0f, -1.0f, 0.0f }, glsl::vec4{ 0.0f, 0.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{ -1.0f,  1.0f, 0.0f }, glsl::vec4{ 1.0f, 1.0f, 0.0f, 1.0f } },
	Vertex{ glsl::vec3{  1.0f,  1.0f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 1.0f, 1.0f } },
};


const std::vector<Vertex> vao_vert2 = {
	Vertex{ glsl::vec3{ -0.5f, -0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 1.0f, 1.0f } },
	Vertex{ glsl::vec3{  0.5f, -0.5f, 0.0f }, glsl::vec4{ 1.0f, 0.0f, 0.0f, 1.0f } },
	Vertex{ glsl::vec3{  0.0f,  0.5f, 0.0f }, glsl::vec4{ 0.0f, 1.0f, 0.0f, 1.0f } },
};

VertexArray vao;
IndexBuffer ibo;
VertexBuffer vbo;

VertexArray vao2;
IndexBuffer ibo2;
VertexBuffer vbo2;

//---------------------------------------------------------------------------
static void vao_init() {

	vao = VertexArray::Create();
	vao2 = VertexArray::Create();

	vbo2 = { vao_vert2 };
	vbo = { vao_vert };

	ibo = { indices };
	ibo2 = { indices2 };

	vao2.AddVertexBuffer(vbo2);
	vao.AddVertexBuffer(vbo);

	vao.SetIndexBuffer(ibo);
	vao2.SetIndexBuffer(ibo2);

	vao.Unbind();
	vbo.Unbind();
	vao2.Unbind();
	vbo2.Unbind();
}

static void PresentBuffer(SDL_Renderer* renderer, SDL_Window* win, SDL_Texture* backBuffer, Shader& shader, float playing_time, float w, float h, int rx, int ry, int rw, int rh) {
	//SDL_GL_BindTexture(backBuffer, NULL, NULL);

	SDL_SetRenderTarget(renderer, backBuffer);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
	SDL_RenderClear(renderer);

	shader.Bind();

	vao.Bind();
	glDrawElements(GL_TRIANGLES, vao.GetIndexBuffer().GetCount(), vao.GetIndexBuffer().GetType(), nullptr);
	vao.Unbind();

	vao2.Bind();
	glDrawElements(GL_TRIANGLES, vao2.GetIndexBuffer().GetCount(), vao2.GetIndexBuffer().GetType(), nullptr);
	vao2.Unbind();

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
	auto renderer = sdl.GetRenderer();
	auto win = sdl.GetWindow();

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
	shader = Shader(ShaderSource{ vertex_source }, ShaderSource{ fragment_source });
	//shader = Shader("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");

	const float WIN_WIDTH = (float)window::GetSize().x;
	const float WIN_HEIGHT = (float)window::GetSize().y;

	GLfloat iResolution[3] = { WIN_WIDTH, WIN_HEIGHT, 0 };
	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	SDL_Texture* texTarget = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);

	int done = 0;
	int useShader = 0;
	SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);

	SDL_Texture* drawTarget = SDL_CreateTexture(renderer.get(), SDL_PIXELFORMAT_RGBA8888,
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
		SDL_SetRenderTarget(renderer.get(), NULL);
		SDL_SetRenderDrawColor(renderer.get(), 255, 255, 255, 255);
		SDL_RenderClear(renderer.get());

		SDL_Point mouse;

		SDL_GetMouseState(&mouse.x, &mouse.y);

		SDL_Rect dest_rect;

		dest_rect.w = WIN_WIDTH;
		dest_rect.h = WIN_HEIGHT;

		dest_rect.x = mouse.x - dest_rect.w / 2;
		dest_rect.y = mouse.y - dest_rect.h / 2;

		curr_time = clock();
		playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

		//DrawRect(renderer.get(), rect1, { 0, 0, 255, 255 });

		PresentBuffer(renderer.get(), win.get(), drawTarget, shader, playtime_in_second, WIN_WIDTH, WIN_HEIGHT, dest_rect.x, dest_rect.y, dest_rect.w, dest_rect.h);

		//DrawRect(renderer.get(), rect2, { 255, 0, 0, 255 });
		SDL_RenderPresent(renderer.get());
		
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

	SDL_DestroyTexture(texTarget);
}



} // namespace impl

} // namespace ptgn