#include <SDL3/SDL.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "platform/window/window.h"
#include "renderer/backend/gl/gl.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/image/surface.h"
#include "renderer/renderer.h"
#include "renderer/resources/vertex.h"

int main() {
	using namespace ptgn;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return 1;
	}

	// OpenGL 3.3 core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	int W = 1280, H = 720;

	ptgn::Window window{ "Title", { 1280, 720 } };
	window.SetSetting(WindowSetting::Shown);

	Renderer renderer{ window };

	bool running = true;

	auto window_size = window.GetSize();

	impl::Surface texture1_surface{ "assets/logo.png" };

	auto texture1 = renderer.gl_->CreateTexture(
		texture1_surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, texture1_surface.size, GL_RGBA
	);

	auto scene_target = renderer.CreateRenderTarget(window_size, TextureFormat::RGBA8);

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		renderer.BeginFrame();

		renderer.BindRenderTarget(scene_target);
		renderer.gl_->ClearToColor(scene_target.framebuffer, color::Blue);
		renderer.SetBlend(BlendMode::Blend);
		renderer.DrawTexture(texture1, { 0, 0 }, renderer.gl_->GetTextureSize(texture1));
		renderer.DrawTexture(texture1, -window_size / 2.0f, renderer.gl_->GetTextureSize(texture1));
		renderer.DrawTexture(texture1, window_size / 2.0f, renderer.gl_->GetTextureSize(texture1));
		renderer.DrawTexture(
			texture1, { window_size.x / 2.0f, -window_size.y / 2.0f },
			renderer.gl_->GetTextureSize(texture1)
		);
		renderer.DrawTexture(
			texture1, { -window_size.x / 2.0f, window_size.y / 2.0f },
			renderer.gl_->GetTextureSize(texture1)
		);
		renderer.DrawTexture(
			texture1, { window_size.x / 2.0f, 0.0f }, renderer.gl_->GetTextureSize(texture1)
		);
		renderer.DrawTexture(
			texture1, { -window_size.x / 2.0f, 0.0f }, renderer.gl_->GetTextureSize(texture1)
		);
		renderer.DrawTexture(
			texture1, { 0.0f, window_size.y / 2.0f }, renderer.gl_->GetTextureSize(texture1)
		);
		renderer.DrawTexture(
			texture1, { 0.0f, -window_size.y / 2.0f }, renderer.gl_->GetTextureSize(texture1)
		);

		auto pass = renderer.BeginPass(scene_target);

		renderer.BindRenderTarget(pass);
		renderer.SetBlend(BlendMode::ReplaceRGBA);

		renderer.SetShader(renderer.gl_->GetShader("isolate_bright"));
		renderer.gl_->SetUniform(renderer.gl_->GetShader("isolate_bright"), "threshold", 0.8f);
		renderer.DrawTexture(renderer.gl_->GetShader("isolate_bright"), pass, scene_target);
		renderer.DrawTexture(renderer.gl_->GetShader("blur"), pass, scene_target);

		renderer.BindRenderTarget(scene_target);
		renderer.SetBlend(BlendMode::AddRGBA);
		renderer.DrawTexture(renderer.gl_->GetShader("quad"), pass, scene_target);

		/*
		renderer.DrawLightQuad({ .position			= { 0.0f, 0.0f },
								 .radius			= 250.0f,
								 .color				= color::Green,
								 .intensity			= 2.0f,
								 .falloff			= 1.0f,
								 .ambient_color		= { 0.1f, 0.1f, 0.1f },
								 .ambient_intensity = 0.2f,
								 .attenuation		= { 1.0f, 0.09f, 0.032f } });
								 */

		// renderer.gl_->SavePNG("debug_images/name_of_png1.png", scene_fbo);

		// renderer.BindRenderTarget(scene_fbo2, { { 0, 0 }, window_size });
		// renderer.gl_->ClearToColor(scene_fbo2, color::Transparent);

		// renderer.gl_->SavePNG("debug_images/grayscale0.png", scene_fbo2);
		/*renderer.DrawTexturedQuad(
			renderer.gl_->GetShader("grayscale"), scene_texture, {},
			renderer.gl_->GetTextureSize(scene_texture)
		);*/
		// renderer.FlushBatch();
		// renderer.gl_->SavePNG("debug_images/grayscale1.png", scene_fbo2);
		// renderer.BindRenderTarget(scene_fbo, { { 0, 0 }, window_size });
		/*renderer.DrawTexturedQuad(
			renderer.gl_->GetShader("screen_default"), scene_texture2, {},
			renderer.gl_->GetTextureSize(scene_texture2)
		);*/

		// renderer.gl_->SavePNG("debug_images/name_of_png2.png", scene_fbo);
		// renderer.FlushBatch();
		// renderer.gl_->SavePNG("debug_images/name_of_png3.png", scene_fbo);

		//   TODO: Add batching of consecutive textures.

		renderer.BindRenderTarget(renderer.screen_target);
		renderer.SetBlend(BlendMode::ReplaceRGBA);
		renderer.DrawTexture(scene_target.color, { 0, 0 }, scene_target.size);

		renderer.EndFrame();

		PTGN_LOG("--------");

		SDL_GL_SwapWindow(window);
	}

	SDL_Quit();
	return 0;
}
