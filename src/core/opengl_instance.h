#pragma once

namespace ptgn {

namespace gl {

class OpenGLInstance {
public:
	OpenGLInstance();
	~OpenGLInstance();
	[[nodiscard]] bool IsInitialized() const;

private:
	bool InitOpenGL();
	bool initialized_{ false };
};

} // namespace gl

} // namespace ptgn