#include "gl_loader.h"

#include <set>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <streambuf>
#include <cassert>

#include "protegon/log.h"
#include "buffer.h"

#define GLE(name, caps_name) PFNGL##caps_name##PROC gl##name;
GL_LIST
#undef GLE

namespace ptgn {

namespace impl {

bool GLInit() {
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

void InitializeFileSystem() {
// TODO: Check if useful:
#ifdef __APPLE__
	/*CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8*)path, PATH_MAX)) {
		std::cerr << "Couldn't get file system representation! " << std::endl;
	}
	CFRelease(resourcesURL);
	chdir(path);*/
#endif
}

} // namespace impl

std::shared_ptr<VertexBuffer<float>> vbo[4] = { {}, {}, {}, {} };
GLuint vao[4] = { 0, 0, 0, 0 };
const std::vector<float> vao_vert = {
	-0.5f, -0.5f, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f,
	 0.5f, -0.5f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f,
	 0.0f,  0.5f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f,
};
//---------------------------------------------------------------------------
void vao_init() {
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
void vao_exit() {
	glDeleteVertexArrays(4, vao);
	//glDeleteBuffers(4, vbo);
}
//---------------------------------------------------------------------------
void vao_draw(GLuint mode) {
}

void PresentBuffer(SDL_Renderer* renderer, SDL_Window* win, SDL_Texture* backBuffer, Shader& shader, float playing_time, float w, float h, int rx, int ry, int rw, int rh) {
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

} // namespace ptgn
