#pragma once

namespace ptgn {

class OpenGLInstance {
public:
	OpenGLInstance();
	~OpenGLInstance();
	bool IsInitialized() const;
private:
	bool initialized_{ false };
	bool InitOpenGL();
};

namespace impl {

void TestOpenGL();

}

} // namespace ptgn