#include "renderer/backend/gl/gl.h"

#include <source_location>
#include <string_view>
#include <vector>

#include "core/config.h"
#include "core/log.h"

#ifdef PTGN_DEBUG

namespace ptgn::impl::gl {

std::string_view GetErrorString(GLenum error) {
	switch (error) {
		case GL_INVALID_ENUM:				   return "Invalid Enum";
		case GL_INVALID_VALUE:				   return "Invalid Value";
		case GL_INVALID_OPERATION:			   return "Invalid Operation";
		case GL_OUT_OF_MEMORY:				   return "Out of Memory";
		case GL_INVALID_FRAMEBUFFER_OPERATION: return "Invalid Frame Buffer Operation";
		case GL_NO_ERROR:
			PTGN_ERROR("Cannot retrieve error string for none type error");
			[[fallthrough]];
		default: PTGN_ERROR("Failed to recognize GL error code: ", error);
	}
}

void HandleErrors(std::source_location location) {
	std::vector<GLenum> errors;
	while (true) {
		GLenum error{ glGetError() };
		if (error == GL_NO_ERROR) {
			break;
		}
		errors.emplace_back(error);
	}
	if (!errors.empty()) {
		for (auto error : errors) {
			auto error_string{ GetErrorString(error) };
			ptgn::impl::DebugMessage("OPENGL ERROR: ", ToString(error_string), location);
		}
		PTGN_ABORT();
	}
}

} // namespace ptgn::impl::gl

#endif