#include "protegon/renderer.h"

#include <SDL.h>

#include "core/game.h"
#include "protegon/texture.h"
#include "protegon/window.h"
#include "renderer/gl_loader.h"

namespace ptgn {

namespace renderer {

void SetDrawColor(const Color& color) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

void ResetDrawColor() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawColor(renderer.get(), 0, 0, 0, 0);
}

void SetBlendMode(BlendMode mode) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawBlendMode(renderer.get(), static_cast<SDL_BlendMode>(mode));
}

void SetDrawMode(const Color& color, BlendMode mode) {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderDrawBlendMode(renderer.get(), static_cast<SDL_BlendMode>(mode));
	SDL_SetRenderDrawColor(renderer.get(), color.r, color.g, color.b, color.a);
}

void Clear() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderClear(renderer.get());
}

void Present() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderPresent(renderer.get());
}

void SetTarget(const Texture& texture) {
	// auto renderer{ global::GetGame().sdl.GetRenderer() };
	// SDL_SetRenderTarget(renderer.get(), texture.GetInstance().get());
}

void ResetTarget() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_SetRenderTarget(renderer.get(), NULL);
}

void DrawTexture(
	const Texture& texture, const Rectangle<int>& destination_rect,
	const Rectangle<int>& source_rect, float angle, Flip flip, V2_int* center_of_rotation
) {
	/*PTGN_CHECK(
		texture.IsValid(), "Cannot draw texture which is uninitialized or destroyed to renderer"
	);
	auto renderer{ global::GetGame().sdl.GetRenderer() };

	SDL_Rect* s_rect	 = NULL;
	SDL_Rect* d_rect	 = NULL;
	SDL_Point* cor_point = NULL;

	SDL_Rect s(source_rect);
	SDL_Rect d(destination_rect);
	SDL_Point p;

	if (!source_rect.IsEmpty()) {
		s_rect = &s;
	}

	if (!destination_rect.IsEmpty()) {
		d_rect = &d;
	}

	if (center_of_rotation != nullptr) {
		p.x		  = center_of_rotation->x;
		p.y		  = center_of_rotation->y;
		cor_point = &p;
	}

	SDL_RenderCopyEx(
		renderer.get(), texture.GetInstance().get(), s_rect, d_rect, angle, cor_point,
		static_cast<SDL_RendererFlip>(flip)
	);*/
}

namespace impl {

void Flush() {
	auto renderer{ global::GetGame().sdl.GetRenderer() };
	SDL_RenderFlush(renderer.get());
}

} // namespace impl

} // namespace renderer

Renderer::Renderer(const V2_int& size) {
	glViewport(0, 0, size.x, size.y);
	glClearColor(1.0f, 1.0f, 1.0f, 0.0f); /* This Will Clear The Background Color To Black */
	glClearDepth(1.0);					  /* Enables Clearing Of The Depth Buffer */
	glDepthFunc(GL_LESS);				  /* The Type Of Depth Test To Do */
	// glEnable(GL_DEPTH_TEST);			  /* Enables Depth Testing */
	glShadeModel(GL_SMOOTH); /* Enables Smooth Color Shading */

	glEnable(GL_BLEND);

	glMatrixMode(GL_PROJECTION);
	glLoadIdentity(); /* Reset The Projection Matrix */

	double aspect = (GLdouble)size.x / size.y;
	glOrtho(-3.0, 3.0, -3.0 / aspect, 3.0 / aspect, 0.0, 1.0);

	glMatrixMode(GL_MODELVIEW);
}

void Renderer::Draw(const VertexArray& va, Shader& shader, const Texture& texture) const {
	/*
	const VertexBuffer& vbo{ va.GetVertexBuffer() };
	if (!vbo.IsValid()) {
		return; // Do not draw VertexArray with no VertexBuffer set.
	}
	PTGN_CHECK(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");

	bool valid_shader{ shader.IsValid() };

	if (valid_shader) {
		shader.Bind();
	}

	va.Bind();
	const IndexBuffer& ibo{ va.GetIndexBuffer() };
	if (ibo.IsValid()) {
		ibo.Bind();
		glDrawElements(
			static_cast<GLenum>(va.GetPrimitiveMode()), ibo.GetCount(),
			static_cast<GLenum>(ibo.GetType()), nullptr
		);
	} else {
		glDrawArrays(static_cast<GLenum>(va.GetPrimitiveMode()), 0, vbo.GetCount());
	}

	va.Unbind();
	if (valid_shader) {
		shader.Unbind();
	}

	*/

	/* Texture coordinate lookup, to make it simple */
	enum {
		MINX,
		MINY,
		MAXX,
		MAXY
	};

	// glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity(); /* Reset The View */

	// glTranslatef(-1.5f, 0.0f, 0.0f);					/* Move Left 1.5 Units */

	///* draw a triangle (in smooth coloring mode) */
	glBegin(GL_POLYGON);			/* start drawing a polygon */
	glColor3f(1.0f, 0.0f, 0.0f);	/* Set The Color To Red */
	glVertex3f(0.0f, 1.0f, 0.0f);	/* Top */
	glColor3f(0.0f, 1.0f, 0.0f);	/* Set The Color To Green */
	glVertex3f(1.0f, -1.0f, 0.0f);	/* Bottom Right */
	glColor3f(0.0f, 0.0f, 1.0f);	/* Set The Color To Blue */
	glVertex3f(-1.0f, -1.0f, 0.0f); /* Bottom Left */
	glEnd();						/* we're done with the polygon (smooth color interpolation) */

	// glTranslatef(3.0f, 0.0f, 0.0f); /* Move Right 3 Units */

	///* Enable blending */
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	///* draw a textured square (quadrilateral) */
	glEnable(GL_TEXTURE_2D);
	texture.Bind();
	//  glBindTexture(GL_TEXTURE_2D, texture);
	// glColor3f(1.0f, 1.0f, 1.0f);
	if (shader.IsValid()) {
		shader.Bind();
	}

	va.Bind();
	glDrawArrays(static_cast<GLenum>(va.GetPrimitiveMode()), 0, 4);
	va.Unbind();

	float texcoord[4];

	glBegin(GL_QUADS); /* start drawing a polygon (4 sided) */
	// glTexCoord2f(texcoord[MINX], texcoord[MINY]);
	glVertex3f(-1.0f, 1.0f, 0.0f); /* Top Left */
	// glTexCoord2f(texcoord[MAXX], texcoord[MINY]);
	glVertex3f(1.0f, 1.0f, 0.0f); /* Top Right */
	// glTexCoord2f(texcoord[MAXX], texcoord[MAXY]);
	glVertex3f(1.0f, -1.0f, 0.0f); /* Bottom Right */
	// glTexCoord2f(texcoord[MINX], texcoord[MAXY]);
	glVertex3f(-1.0f, -1.0f, 0.0f); /* Bottom Left */
	glEnd();						/* done with the polygon */

	if (shader.IsValid()) {
		shader.Unbind();
	}
	texture.Unbind();
	glDisable(GL_TEXTURE_2D);

	// SDL_GL_SwapWindow(global::GetGame().sdl.GetWindow().get());
	///* swap buffers to display, since we're double buffered. */
}

void Renderer::Clear() const {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::Present() const {
	SDL_GL_SwapWindow(global::GetGame().sdl.GetWindow().get());
}

} // namespace ptgn