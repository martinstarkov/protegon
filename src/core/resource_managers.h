#pragma once

#include "core/handle_manager.h"
#include "protegon/audio.h"
#include "protegon/font.h"
#include "protegon/scene.h"
#include "protegon/shader.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "utility/time.h"

namespace ptgn {

class Game;

class FontManager : public HandleManager<Font> {
private:
	FontManager()							   = default;
	~FontManager()							   = default;
	FontManager(const FontManager&)			   = delete;
	FontManager(FontManager&&)				   = default;
	FontManager& operator=(const FontManager&) = delete;
	FontManager& operator=(FontManager&&)	   = default;

public:
private:
	friend class Game;
};

class TextManager : public HandleManager<Text> {
private:
	TextManager()							   = default;
	~TextManager()							   = default;
	TextManager(const TextManager&)			   = delete;
	TextManager(TextManager&&)				   = default;
	TextManager& operator=(const TextManager&) = delete;
	TextManager& operator=(TextManager&&)	   = default;

public:
private:
	friend class Game;
};

class TextureManager : public HandleManager<Texture> {
private:
	TextureManager()								 = default;
	~TextureManager()								 = default;
	TextureManager(const TextureManager&)			 = delete;
	TextureManager(TextureManager&&)				 = default;
	TextureManager& operator=(const TextureManager&) = delete;
	TextureManager& operator=(TextureManager&&)		 = default;

public:
private:
	friend class Game;
};

class ShaderManager : public HandleManager<Shader> {
private:
	ShaderManager()								   = default;
	~ShaderManager()							   = default;
	ShaderManager(const ShaderManager&)			   = delete;
	ShaderManager(ShaderManager&&)				   = default;
	ShaderManager& operator=(const ShaderManager&) = delete;
	ShaderManager& operator=(ShaderManager&&)	   = default;

public:
private:
	friend class Game;
};

using FontKey	 = std::size_t;
using MusicKey	 = std::size_t;
using SoundKey	 = std::size_t;
using TextKey	 = std::size_t;
using TextureKey = std::size_t;
using ShaderKey	 = std::size_t;

class MusicManager : public HandleManager<Music> {
private:
	MusicManager()								 = default;
	~MusicManager()								 = default;
	MusicManager(const MusicManager&)			 = delete;
	MusicManager(MusicManager&&)				 = default;
	MusicManager& operator=(const MusicManager&) = delete;
	MusicManager& operator=(MusicManager&&)		 = default;

public:
	void Pause();
	void Resume();
	// Returns the current music track volume from 0 to 128 (MIX_MAX_VOLUME).
	[[nodiscard]] int GetVolume();
	// Volume can be set from 0 to 128 (MIX_MAX_VOLUME).
	void SetVolume(int new_volume);
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Toggle(int optional_new_volume = -1);
	// Sets volume to 0.
	void Mute();
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Unmute(int optional_new_volume = -1);
	void Stop();
	void FadeOut(milliseconds time);
	[[nodiscard]] bool IsPlaying();
	[[nodiscard]] bool IsPaused();
	[[nodiscard]] bool IsFading();

private:
	friend class Game;
};

class SoundManager : public HandleManager<Sound> {
private:
	SoundManager()								 = default;
	~SoundManager()								 = default;
	SoundManager(const SoundManager&)			 = delete;
	SoundManager(SoundManager&&)				 = default;
	SoundManager& operator=(const SoundManager&) = delete;
	SoundManager& operator=(SoundManager&&)		 = default;

public:
	void HaltChannel(int channel);
	void ResumeChannel(int channel);

private:
	friend class Game;
};

} // namespace ptgn