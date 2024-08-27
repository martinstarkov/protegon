#include "renderer/gl_helper.h"

#include "renderer/gl_loader.h"

namespace ptgn {

namespace impl {

void GLClearErrors() {
	while (gl::glGetError() != static_cast<gl::GLenum>(GLError::None)) {}
}

std::vector<GLError> GLGetErrors() {
	std::vector<GLError> errors;
	while (gl::GLenum error = gl::glGetError()) {
		GLError e = static_cast<GLError>(error);
		if (e == GLError::None) {
			break;
		}
		errors.push_back(e);
	}
	return errors;
}

std::string GLGetErrorString(GLError error) {
	PTGN_ASSERT(error != GLError::None, "Cannot retrieve error string for none type error");
	switch (error) {
		case GLError::InvalidEnum:		return "Invalid Enum";
		case GLError::InvalidValue:		return "Invalid Value";
		case GLError::InvalidOperation: return "Invalid Operation";
		case GLError::StackOverflow:	return "Stack Overflow";
		case GLError::StackUnderflow:	return "Stack Underflow";
		case GLError::OutOfMemory:		return "Out of Memory";
		case GLError::None:
		default:						PTGN_ERROR("Failed to recognize GL error code");
	}
}

void GLPrintErrors(
	const std::string& function_name, const path& filepath, std::size_t line,
	const std::vector<GLError>& errors
) {
	for (auto error : errors) {
		ptgn::debug::Print(
			"OpenGL Error: ", filepath.filename().string(), ":", line, ": ", function_name, ": ",
			GLGetErrorString(error)
		);
	}
}

} // namespace impl

} // namespace ptgn
