#pragma once

#include <cstdint>
#include <memory>

#include "renderer/gl/gl_handle.h"
#include "serialization/json/json.h"

struct _Mix_Music;
using Mix_Music = _Mix_Music;
struct Mix_Chunk;

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

namespace ptgn {

namespace impl {

struct Mix_MusicDeleter {
	void operator()(Mix_Music* music) const;
};

struct Mix_ChunkDeleter {
	void operator()(Mix_Chunk* sound) const;
};

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

struct ShaderAsset {
	gl::Handle<gl::Shader> shader;
};

struct TextureAsset {
	gl::Handle<gl::Texture> texture;
};

struct MusicAsset {
	std::unique_ptr<Mix_Music, Mix_MusicDeleter> music;
};

struct SoundAsset {
	std::unique_ptr<Mix_Chunk, Mix_ChunkDeleter> sound;
};

struct FontAsset {
	std::int32_t pt_size{ 0 };
	std::unique_ptr<TTF_Font, TTF_FontDeleter> font;
};

struct JsonAsset {
	json j;
};

} // namespace impl

} // namespace ptgn