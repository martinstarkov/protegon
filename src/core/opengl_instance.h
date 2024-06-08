#pragma once

namespace ptgn {

class OpenGLInstance {
public:
	OpenGLInstance();
	~OpenGLInstance();
	[[nodiscard]] bool IsInitialized() const;
private:
	bool initialized_{ false };
	bool InitOpenGL();
};

namespace impl {

void TestOpenGL();

} // namespace impl

} // namespace ptgn