#pragma once

#include "core/util/time.h"

namespace ptgn::impl {

class SDLInstance {
public:
	SDLInstance()							   = default;
	~SDLInstance()							   = default;
	SDLInstance(SDLInstance&&)				   = default;
	SDLInstance& operator=(SDLInstance&&)	   = default;
	SDLInstance(const SDLInstance&)			   = delete;
	SDLInstance& operator=(const SDLInstance&) = delete;

	[[nodiscard]] bool IsInitialized() const;
	[[nodiscard]] bool SDLMixerIsInitialized() const;
	[[nodiscard]] bool SDLTTFIsInitialized() const;
	[[nodiscard]] bool SDLIsInitialized() const;
	[[nodiscard]] bool SDLImageIsInitialized() const;

	void Init();
	void Shutdown();

	static void Delay(milliseconds time);

private:
	void InitSDL();
	void InitSDLImage();
	void InitSDLTTF();
	void InitSDLMixer();

	bool sdl_mixer_init_{ false };
	bool sdl_ttf_init_{ false };
	bool sdl_image_init_{ false };
	bool sdl_init_{ false };
};

} // namespace ptgn::impl