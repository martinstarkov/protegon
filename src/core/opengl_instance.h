#pragma once

namespace ptgn {

class OpenGLInstance {
public:
	OpenGLInstance();
	~OpenGLInstance();
	[[nodiscard]] bool IsInitialized() const;

private:
	bool InitOpenGL();
	bool initialized_{ false };
};

} // namespace ptgn