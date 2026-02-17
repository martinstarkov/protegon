#include "runtime/asset/asset_manager.h"

#include <SDL3/SDL_error.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "app/application.h"
#include "core/assert.h"
#include "core/util/file.h"
#include "renderer/backend/gl/gl_context.h"
#include "renderer/image/surface.h"
#include "renderer/primitives/font.h"
#include "runtime/audio/audio.h"
#include "serialization/json/json.h"

// TODO: Add async asset loading.

namespace ptgn {

AssetManager::AssetManager(impl::SDLInstance& sdl, impl::gl::GLContext& gl) :
	sdl_{ sdl }, gl_{ gl } {}

Shader AssetManager::LoadShader(
	const std::variant<ShaderCode, path>& source, const std::string& shader_name
) {
	return gl_.CreateShader(source, shader_name);
}

Shader AssetManager::LoadShader(
	const std::variant<ShaderCode, std::string>& vertex,
	const std::variant<ShaderCode, std::string>& fragment, const std::string& shader_name
) {
	return gl_.CreateShader(vertex, fragment, shader_name);
}

Texture AssetManager::LoadTexture(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create texture from invalid path: ", asset_path.string()
	);

	impl::Surface surface{ asset_path };

	return gl_.CreateTexture(
		surface.pixels.data(), GL_RGBA, GL_UNSIGNED_BYTE, surface.size, GL_RGBA
	);
}

Font AssetManager::LoadFont(const path& asset_path, float pt_size) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create font from invalid path: ", asset_path.string()
	);

	auto ttf_font = TTF_OpenFont(asset_path.string().c_str(), pt_size);

	PTGN_ASSERT(ttf_font, SDL_GetError());

	std::shared_ptr<TTF_Font> font{ ttf_font, impl::TTF_FontDeleter{} };

	return Font{ font, pt_size };
}

Audio AssetManager::LoadAudio(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create audio from invalid path: ", asset_path.string()
	);

	PTGN_ASSERT(sdl_.mixer_, "Cannot load audio when SDL_mixer has not been created");

	auto mix_audio = MIX_LoadAudio(sdl_.mixer_, asset_path.string().c_str(), true);

	PTGN_ASSERT(mix_audio, SDL_GetError());

	std::shared_ptr<MIX_Audio> music{ mix_audio, impl::MIX_AudioDeleter{} };

	return Audio{ music };
}

json AssetManager::LoadJson(const path& asset_path) {
	PTGN_ASSERT(
		FileExists(asset_path), "Cannot create json from invalid path: ", asset_path.string()
	);

	return ptgn::LoadJson(asset_path);
}

} // namespace ptgn