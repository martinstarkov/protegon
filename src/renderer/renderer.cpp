#include "protegon/renderer.h"

#include "SDL.h"

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
	gl::glViewport(0, 0, size.x, size.y);
	gl::glClearColor(0.0f, 0.0f, 0.0f, 0.0f); /* This Will Clear The Background Color To Black */
	gl::glClearDepth(1.0);					  /* Enables Clearing Of The Depth Buffer */
	gl::glDepthFunc(GL_LESS);				  /* The Type Of Depth Test To Do */
	gl::glEnable(GL_DEPTH_TEST);			  /* Enables Depth Testing */
	gl::glShadeModel(GL_SMOOTH);			  /* Enables Smooth Color Shading */

	// From: https://nullprogram.com/blog/2023/01/08/
	// If you’re using OpenGL, set a non-zero SDL_GL_SetSwapInterval so that SDL_GL_SwapWindow synchronizes. For the other rendering APIs, consult their documentation. (I can only speak to SDL and OpenGL from experience
	SDL_GL_SetSwapInterval(1);

	gl::glEnable(GL_BLEND);
	gl::glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //gl::glMatrixMode(GL_PROJECTION);
	//gl::glLoadIdentity(); /* Reset The Projection Matrix */

	//double aspect = (gl::GLdouble)size.x / size.y;
	//gl::glOrtho(-3.0, 3.0, -3.0 / aspect, 3.0 / aspect, 0.0, 1.0);

	//gl::glMatrixMode(GL_MODELVIEW);
}

void Renderer::Draw(const VertexArray& va, Shader& shader, const Texture& texture, int slot) const {
	const VertexBuffer& vbo{ va.GetVertexBuffer() };
	if (!vbo.IsValid()) {
		return; // Do not draw VertexArray with no VertexBuffer set.
	}
	PTGN_CHECK(va.IsValid(), "Cannot draw uninitialized or destroyed vertex array");

	bool valid_shader{ shader.IsValid() };
	bool valid_texture{ texture.IsValid() };

	if (valid_shader) {
		shader.Bind();
	}

	va.Bind();
	const IndexBuffer& ibo{ va.GetIndexBuffer() };
	if (ibo.IsValid()) {
		ibo.Bind();
		if (valid_texture) {
			gl::glEnable(GL_TEXTURE_2D);
			texture.Bind(slot);
			shader.SetUniform("tex0", slot);
		}
		gl::glDrawElements(
			static_cast<gl::GLenum>(va.GetPrimitiveMode()), ibo.GetCount(),
			static_cast<gl::GLenum>(impl::IndexBufferInstance::GetType()), nullptr
		);
	} else {
		if (valid_texture) {
			gl::glEnable(GL_TEXTURE_2D);
			texture.Bind(slot);
			shader.SetUniform("tex0", slot);
		}
		gl::glDrawArrays(static_cast<gl::GLenum>(va.GetPrimitiveMode()), 0, vbo.GetCount());
	}

	if (valid_texture) {
		texture.Unbind();
		gl::glDisable(GL_TEXTURE_2D);
	}

	va.Unbind();

	if (valid_shader) {
		shader.Unbind();
	}
}

void Renderer::Clear() const {
	gl::glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Renderer::Present() const {
	SDL_GL_SwapWindow(global::GetGame().sdl.GetWindow().get());
}

} // namespace ptgn