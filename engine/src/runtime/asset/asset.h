#pragma once

#include <cstdint>
#include <memory>

#include "renderer/resources/shader.h"
#include "renderer/resources/texture.h"
#include "serialization/json/json.h"

struct MIX_Audio;

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif

namespace ptgn::impl {

struct MIX_AudioDeleter {
	void operator()(MIX_Audio* audio) const;
};

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

struct ShaderAsset {
	operator Shader() const {
		return shader;
	}

	Shader shader;
};

struct TextureAsset {
	operator Texture() const {
		return texture;
	}

	Texture texture;
};

struct AudioAsset {
	std::unique_ptr<MIX_Audio, MIX_AudioDeleter> audio;
};

struct FontAsset {
	std::unique_ptr<TTF_Font, TTF_FontDeleter> font;
	float pt_size{ 0.0f };
};

struct JsonAsset {
	json j;
};

} // namespace ptgn::impl