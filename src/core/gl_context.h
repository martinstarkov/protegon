#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "protegon/file.h"

namespace ptgn {

namespace impl {

enum class GLError {
	None			 = 0,	   // GL_NO_ERROR
	InvalidEnum		 = 0x0500, // GL_INVALID_ENUM
	InvalidValue	 = 0x0501, // GL_INVALID_VALUE
	InvalidOperation = 0x0502, // GL_INVALID_OPERATION
	StackOverflow	 = 0x0503, // GL_STACK_OVERFLOW
	StackUnderflow	 = 0x0504, // GL_STACK_UNDERFLOW
	OutOfMemory		 = 0x0505, // GL_OUT_OF_MEMORY
};

struct GLVersion {
	GLVersion();

	int major{ 0 };
	int minor{ 0 };
};

// Must be constructed after SDL_Window has been created.
class GLContext {
public:
	GLContext()							   = default;
	~GLContext()						   = default;
	GLContext(const GLContext&)			   = delete;
	GLContext(GLContext&&)				   = default;
	GLContext& operator=(const GLContext&) = delete;
	GLContext& operator=(GLContext&&)	   = default;

	[[nodiscard]] bool IsInitialized() const;

	void Init();
	void Shutdown();

	static void ClearErrors();

	[[nodiscard]] static std::vector<GLError> GetErrors();

	[[nodiscard]] static std::string_view GetErrorString(GLError error);

	static void PrintErrors(
		std::string_view function_name, const path& filepath, std::size_t line,
		const std::vector<GLError>& errors
	);

private:
	static void LoadGLFunctions();

	void* context_{ nullptr };
};

} // namespace impl

} // namespace ptgn