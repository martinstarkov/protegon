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

GLuint vbo[4] = { -1,-1,-1,-1 };
GLuint vao[4] = { -1,-1,-1,-1 };
const float vao_pos[] =
{
	//       x      y     z
		 0.75f, 0.75f, 0.0f,
		 0.75f,-0.75f, 0.0f,
		-0.75f,-0.75f, 0.0f,
};
const float vao_col[] =
{
	//      r   g    b
		 1.0f,0.0f,0.0f,
		 0.0f,1.0f,0.0f,
		 0.0f,0.0f,1.0f,
};
//---------------------------------------------------------------------------
void vao_init() {
	glGenVertexArrays(4, vao);
	glGenBuffers(4, vbo);

	glBindVertexArray(vao[0]);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vao_pos), vao_pos, GL_STATIC_DRAW);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vao_col), vao_col, GL_STATIC_DRAW);
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, 0);

	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
}
//---------------------------------------------------------------------------
void vao_exit() {
	glDeleteVertexArrays(4, vao);
	glDeleteBuffers(4, vbo);
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
	SDL_Rect dest_rect{ rx, ry, rw, rh };
	SDL_RenderCopyEx(renderer, backBuffer, NULL, &dest_rect, 0, NULL, SDL_FLIP_NONE);
}

} // namespace ptgn
