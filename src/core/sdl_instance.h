#pragma once

namespace ptgn {

namespace impl {

class SDLInstance {
public:
	SDLInstance();
	~SDLInstance();
	SDLInstance(SDLInstance&&)				   = default;
	SDLInstance& operator=(SDLInstance&&)	   = default;
	SDLInstance(const SDLInstance&)			   = delete;
	SDLInstance& operator=(const SDLInstance&) = delete;

private:
	static void InitSDL();
	static void InitSDLImage();
	static void InitSDLTTF();
	static void InitSDLMixer();
};

} // namespace impl

} // namespace ptgn