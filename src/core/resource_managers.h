#pragma once

#include <unordered_set>

#include "core/manager.h"
#include "protegon/audio.h"
#include "protegon/font.h"
#include "protegon/shader.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "protegon/tween.h"
#include "utility/time.h"

namespace ptgn {

class Game;

class TweenManager : public Manager<Tween> {
private:
	TweenManager() = default;

	~TweenManager() override = default;

	TweenManager(const TweenManager&)			 = delete;
	TweenManager(TweenManager&&)				 = default;
	TweenManager& operator=(const TweenManager&) = delete;
	TweenManager& operator=(TweenManager&&)		 = default;

public:
	void KeepAlive(const Key& key) {
		keep_alive_tweens_.insert(key);
	}

	void Unload(const Key& key) {
		Manager::Unload(key);
		keep_alive_tweens_.erase(key);
	}

	void Clear() {
		Manager::Clear();
		keep_alive_tweens_ = {};
	}

	void Reset() {
		Manager::Reset();
		keep_alive_tweens_ = {};
	}

private:
	friend class Game;

	std::unordered_set<Key> keep_alive_tweens_;

	void Update(float dt);
};

class FontManager : public Manager<Font> {
private:
	FontManager()								   = default;
	~FontManager() override						   = default;
	FontManager(const FontManager&)				   = delete;
	FontManager(FontManager&&) noexcept			   = default;
	FontManager& operator=(const FontManager&)	   = delete;
	FontManager& operator=(FontManager&&) noexcept = default;

private:
	friend class Game;
};

class TextManager : public Manager<Text> {
private:
	TextManager()								   = default;
	~TextManager() override						   = default;
	TextManager(const TextManager&)				   = delete;
	TextManager(TextManager&&) noexcept			   = default;
	TextManager& operator=(const TextManager&)	   = delete;
	TextManager& operator=(TextManager&&) noexcept = default;

private:
	friend class Game;
};

class TextureManager : public Manager<Texture> {
private:
	TextureManager()									 = default;
	~TextureManager() override							 = default;
	TextureManager(const TextureManager&)				 = delete;
	TextureManager(TextureManager&&) noexcept			 = default;
	TextureManager& operator=(const TextureManager&)	 = delete;
	TextureManager& operator=(TextureManager&&) noexcept = default;

private:
	friend class Game;
};

class ShaderManager : public Manager<Shader> {
private:
	ShaderManager()									   = default;
	~ShaderManager() override						   = default;
	ShaderManager(const ShaderManager&)				   = delete;
	ShaderManager(ShaderManager&&) noexcept			   = default;
	ShaderManager& operator=(const ShaderManager&)	   = delete;
	ShaderManager& operator=(ShaderManager&&) noexcept = default;

private:
	friend class Game;
};

using FontKey	 = std::size_t;
using MusicKey	 = std::size_t;
using SoundKey	 = std::size_t;
using TextKey	 = std::size_t;
using TextureKey = std::size_t;
using ShaderKey	 = std::size_t;

class MusicManager : public Manager<Music> {
private:
	MusicManager()									 = default;
	~MusicManager() override						 = default;
	MusicManager(const MusicManager&)				 = delete;
	MusicManager(MusicManager&&) noexcept			 = default;
	MusicManager& operator=(const MusicManager&)	 = delete;
	MusicManager& operator=(MusicManager&&) noexcept = default;

public:
	void Pause() const;
	void Resume() const;
	// Returns the current music track volume from 0 to 128 (MIX_MAX_VOLUME).
	[[nodiscard]] int GetVolume() const;
	// Volume can be set from 0 to 128 (MIX_MAX_VOLUME).
	void SetVolume(int new_volume) const;
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Toggle(int optional_new_volume = -1) const;
	// Sets volume to 0.
	void Mute() const;
	// Default: -1 for max volume 128 (MIX_MAX_VOLUME).
	void Unmute(int optional_new_volume = -1) const;
	void Stop() const;
	void FadeOut(milliseconds time) const;
	[[nodiscard]] bool IsPlaying() const;
	[[nodiscard]] bool IsPaused() const;
	[[nodiscard]] bool IsFading() const;

private:
	friend class Game;
};

class SoundManager : public Manager<Sound> {
private:
	SoundManager()									 = default;
	~SoundManager() override						 = default;
	SoundManager(const SoundManager&)				 = delete;
	SoundManager(SoundManager&&) noexcept			 = default;
	SoundManager& operator=(const SoundManager&)	 = delete;
	SoundManager& operator=(SoundManager&&) noexcept = default;

public:
	void HaltChannel(int channel) const;
	void ResumeChannel(int channel) const;
	void FadeOut(int channel, milliseconds time) const;

private:
	friend class Game;
};

} // namespace ptgn