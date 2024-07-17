#pragma once

#include "core/handle_manager.h"
#include "protegon/font.h"
#include "protegon/scene.h"
#include "protegon/shader.h"
#include "protegon/audio.h"
#include "protegon/text.h"
#include "protegon/texture.h"
#include "utility/time.h"

namespace ptgn {

using FontManager = HandleManager<Font>;
using TextManager = HandleManager<Text>;
using TextureManager = HandleManager<Texture>;
using ShaderManager = HandleManager<Shader>;

using FontKey	  = std::size_t;
using MusicKey	  = std::size_t;
using SoundKey	  = std::size_t;
using TextKey	  = std::size_t;
using TextureKey  = std::size_t;
using ShaderKey	  = std::size_t;

class MusicManager : public HandleManager<Music> {
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
};

class SoundManager : public HandleManager<Sound> {
public:
	void HaltChannel(int channel);
	void ResumeChannel(int channel);
};

} // namespace ptgn