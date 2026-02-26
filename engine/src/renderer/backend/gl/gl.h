#pragma once

#include <source_location>
#include <string_view>

#include "core/config.h"
#include "platform/platform.h"

// IMPORTANT: This file is not meant to be included outside the protegon library
// so keep it in .cpp files only!

#ifdef __EMSCRIPTEN__

#include <SDL3/SDL_opengles2.h> // GLES2 / WebGL-style API

typedef void(GL_APIENTRYP PFNGLVERTEXATTRIBIPOINTERPROC)(
	GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer
);

typedef void(GL_APIENTRYP PFNGLCLEARBUFFERFVPROC)(
	GLenum buffer, GLint drawbuffer, const GLfloat* value
);

typedef void(GL_APIENTRYP PFNGLCLEARBUFFERUIVPROC)(
	GLenum buffer, GLint drawbuffer, const GLuint* value
);

#define glClearDepth glClearDepthf
#define glDepthRange glDepthRangef

#ifndef GL_FRAMEBUFFER_UNDEFINED
#define GL_FRAMEBUFFER_UNDEFINED 0x8219
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
#endif

#ifndef GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS 0x8DA8
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#ifndef GL_RGBA8
#define GL_RGBA8 0x8058
#endif

#ifndef GL_DOUBLE
#define GL_DOUBLE 0x140A
#endif

#ifndef GL_HALF_FLOAT
#define GL_HALF_FLOAT 0x140B
#endif

#ifndef GL_SRGB8_ALPHA8
#define GL_SRGB8_ALPHA8 0x8C43
#endif

#ifndef GL_RGBA16F
#define GL_RGBA16F 0x881A
#endif

#ifndef GL_RGBA32F
#define GL_RGBA32F 0x8814
#endif

#ifndef GL_RG16F
#define GL_RG16F 0x822F
#endif

#ifndef GL_R32F
#define GL_R32F 0x822E
#endif

#ifndef GL_RG32F
#define GL_RG32F 0x8230
#endif

#ifndef GL_R11F_G11F_B10F
#define GL_R11F_G11F_B10F 0x8C3A
#endif

#ifndef GL_UNSIGNED_INT_10F_11F_11F_REV
#define GL_UNSIGNED_INT_10F_11F_11F_REV 0x8C3B
#endif

#ifndef GL_RGB10_A2
#define GL_RGB10_A2 0x8059
#endif

#ifndef GL_UNSIGNED_INT_2_10_10_10_REV
#define GL_UNSIGNED_INT_2_10_10_10_REV 0x8368
#endif

#ifndef GL_DEPTH_COMPONENT24
#define GL_DEPTH_COMPONENT24 0x81A6
#endif

#ifndef GL_DEPTH_COMPONENT32F
#define GL_DEPTH_COMPONENT32F 0x8CAC
#endif

#ifndef GL_DEPTH24_STENCIL8
#define GL_DEPTH24_STENCIL8 0x88F0
#endif

#ifndef GL_DEPTH_STENCIL
#define GL_DEPTH_STENCIL 0x84F9
#endif

#ifndef GL_DEPTH_STENCIL_ATTACHMENT
#define GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#endif

#ifndef GL_UNSIGNED_INT_24_8
#define GL_UNSIGNED_INT_24_8 0x84FA
#endif

#ifndef GL_DEPTH32F_STENCIL8
#define GL_DEPTH32F_STENCIL8 0x8CAD
#endif

#ifndef GL_FLOAT_32_UNSIGNED_INT_24_8_REV
#define GL_FLOAT_32_UNSIGNED_INT_24_8_REV 0x8DAD
#endif

#ifndef GL_STENCIL_INDEX
#define GL_STENCIL_INDEX 0x1901
#endif

#ifndef GL_RG8
#define GL_RG8 0x822B
#endif

#ifndef GL_RG
#define GL_RG 0x8227
#endif

#ifndef GL_R8
#define GL_R8 0x8229
#endif

#ifndef GL_RED
#define GL_RED 0x1903
#endif

#ifndef GL_R16F
#define GL_R16F 0x822D
#endif

#ifndef GL_GREEN
#define GL_GREEN 0x1904
#endif

#ifndef GL_BLUE
#define GL_BLUE 0x1905
#endif

#ifndef GL_BGR
#define GL_BGR 0x80E0
#endif

#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif

#ifndef GL_MAX_COLOR_ATTACHMENTS
#define GL_MAX_COLOR_ATTACHMENTS 0x8CDF
#endif

#elif defined(PTGN_PLATFORM_MACOS)

#define GL_SILENCE_DEPRECATION

#include <OpenGL/gl3.h>
#include <OpenGL/gl3ext.h>

#define CompileShader			glCompileShader
#define ShaderSource			glShaderSource
#define ClearBufferfv			glClearBufferfv
#define ClearBufferuiv			glClearBufferuiv
#define GenBuffers				glGenBuffers
#define DeleteBuffers			glDeleteBuffers
#define GetBufferParameteriv	glGetBufferParameteriv
#define BufferData				glBufferData
#define BufferSubData			glBufferSubData
#define BindBuffer				glBindBuffer
#define GenFramebuffers			glGenFramebuffers
#define RenderbufferStorage		glRenderbufferStorage
#define BindRenderbuffer		glBindRenderbuffer
#define GenRenderbuffers		glGenRenderbuffers
#define DeleteRenderbuffers		glDeleteRenderbuffers
#define BindFramebuffer			glBindFramebuffer
#define FramebufferTexture2D	glFramebufferTexture2D
#define FramebufferRenderbuffer glFramebufferRenderbuffer
#define CheckFramebufferStatus	glCheckFramebufferStatus
#define DeleteFramebuffers		glDeleteFramebuffers
#define ActiveTexture			glActiveTexture
#define GenerateMipmap			glGenerateMipmap
#define GenVertexArrays			glGenVertexArrays
#define DeleteVertexArrays		glDeleteVertexArrays
#define BindVertexArray			glBindVertexArray
#define EnableVertexAttribArray glEnableVertexAttribArray
#define VertexAttribIPointer	glVertexAttribIPointer
#define VertexAttribPointer		glVertexAttribPointer
#define CreateProgram			glCreateProgram
#define DeleteProgram			glDeleteProgram
#define ValidateProgram			glValidateProgram
#define UseProgram				glUseProgram
#define LinkProgram				glLinkProgram
#define CreateShader			glCreateShader
#define DeleteShader			glDeleteShader
#define GetShaderiv				glGetShaderiv
#define GetProgramiv			glGetProgramiv
#define GetUniformLocation		glGetUniformLocation
#define GetShaderInfoLog		glGetShaderInfoLog
#define GetProgramInfoLog		glGetProgramInfoLog
#define AttachShader			glAttachShader
#define Uniform1f				glUniform1f
#define Uniform2f				glUniform2f
#define Uniform3f				glUniform3f
#define Uniform4f				glUniform4f
#define Uniform1iv				glUniform1iv
#define Uniform1fv				glUniform1fv
#define Uniform1i				glUniform1i
#define Uniform2i				glUniform2i
#define Uniform3i				glUniform3i
#define Uniform4i				glUniform4i
#define UniformMatrix4fv		glUniformMatrix4fv
#define BlendEquationSeparate	glBlendEquationSeparate
#define BlendFuncSeparate		glBlendFuncSeparate

#else

#include <SDL3/SDL_opengl.h> // Desktop GL
#include <SDL3/SDL_opengl_glext.h>

#endif

#ifndef PTGN_PLATFORM_MACOS

// Adds ##EXTPROC at the end (emscripten only).
#define GL_LIST_3                   \
	GLE(TexStorage2D, TEXSTORAGE2D) \
	/* end */

// Adds ##OESPROC at the end (emscripten only).
#define GL_LIST_2                               \
	GLE(BindVertexArray, BINDVERTEXARRAY)       \
	GLE(GenVertexArrays, GENVERTEXARRAYS)       \
	GLE(DeleteVertexArrays, DELETEVERTEXARRAYS) \
	/* end */

// Adds ##PROC at the end.
#define GL_LIST_1                                           \
	GLE(AttachShader, ATTACHSHADER)                         \
	GLE(BindBuffer, BINDBUFFER)                             \
	GLE(ClearBufferfv, CLEARBUFFERFV)                       \
	GLE(ClearBufferuiv, CLEARBUFFERUIV)                     \
	GLE(BindFramebuffer, BINDFRAMEBUFFER)                   \
	GLE(GetBufferParameteriv, GETBUFFERPARAMETERIV)         \
	GLE(VertexAttribPointer, VERTEXATTRIBPOINTER)           \
	GLE(VertexAttribIPointer, VERTEXATTRIBIPOINTER)         \
	GLE(GenerateMipmap, GENERATEMIPMAP)                     \
	GLE(BufferData, BUFFERDATA)                             \
	GLE(ActiveTexture, ACTIVETEXTURE)                       \
	GLE(BufferSubData, BUFFERSUBDATA)                       \
	GLE(CheckFramebufferStatus, CHECKFRAMEBUFFERSTATUS)     \
	GLE(CompileShader, COMPILESHADER)                       \
	GLE(CreateProgram, CREATEPROGRAM)                       \
	GLE(CreateShader, CREATESHADER)                         \
	GLE(DeleteBuffers, DELETEBUFFERS)                       \
	GLE(DeleteFramebuffers, DELETEFRAMEBUFFERS)             \
	GLE(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY)   \
	GLE(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D)         \
	GLE(GenBuffers, GENBUFFERS)                             \
	GLE(GenRenderbuffers, GENRENDERBUFFERS)                 \
	GLE(DeleteRenderbuffers, DELETERENDERBUFFERS)           \
	GLE(FramebufferRenderbuffer, FRAMEBUFFERRENDERBUFFER)   \
	GLE(RenderbufferStorage, RENDERBUFFERSTORAGE)           \
	GLE(BindRenderbuffer, BINDRENDERBUFFER)                 \
	GLE(GenFramebuffers, GENFRAMEBUFFERS)                   \
	GLE(GetAttribLocation, GETATTRIBLOCATION)               \
	GLE(GetShaderInfoLog, GETSHADERINFOLOG)                 \
	GLE(GetProgramInfoLog, GETPROGRAMINFOLOG)               \
	GLE(GetShaderiv, GETSHADERIV)                           \
	GLE(GetProgramiv, GETPROGRAMIV)                         \
	GLE(DeleteShader, DELETESHADER)                         \
	GLE(GetUniformLocation, GETUNIFORMLOCATION)             \
	GLE(LinkProgram, LINKPROGRAM)                           \
	GLE(ValidateProgram, VALIDATEPROGRAM)                   \
	GLE(DeleteProgram, DELETEPROGRAM)                       \
	GLE(ShaderSource, SHADERSOURCE)                         \
	GLE(UseProgram, USEPROGRAM)                             \
	GLE(BlendEquationSeparate, BLENDEQUATIONSEPARATE)       \
	GLE(BlendFuncSeparate, BLENDFUNCSEPARATE)               \
	GLE(StencilOpSeparate, STENCILOPSEPARATE)               \
	GLE(StencilFuncSeparate, STENCILFUNCSEPARATE)           \
	GLE(StencilMaskSeparate, STENCILMASKSEPARATE)           \
	GLE(BindAttribLocation, BINDATTRIBLOCATION)             \
	GLE(DetachShader, DETACHSHADER)                         \
	GLE(DisableVertexAttribArray, DISABLEVERTEXATTRIBARRAY) \
	GLE(GetActiveAttrib, GETACTIVEATTRIB)                   \
	GLE(GetActiveUniform, GETACTIVEUNIFORM)                 \
	GLE(GetAttachedShaders, GETATTACHEDSHADERS)             \
	GLE(GetUniformfv, GETUNIFORMFV)                         \
	GLE(GetUniformiv, GETUNIFORMIV)                         \
	GLE(GetVertexAttribfv, GETVERTEXATTRIBFV)               \
	GLE(GetVertexAttribiv, GETVERTEXATTRIBIV)               \
	GLE(GetVertexAttribPointerv, GETVERTEXATTRIBPOINTERV)   \
	GLE(IsProgram, ISPROGRAM)                               \
	GLE(IsShader, ISSHADER)                                 \
	GLE(Uniform1f, UNIFORM1F)                               \
	GLE(Uniform2f, UNIFORM2F)                               \
	GLE(Uniform3f, UNIFORM3F)                               \
	GLE(Uniform4f, UNIFORM4F)                               \
	GLE(Uniform1i, UNIFORM1I)                               \
	GLE(Uniform2i, UNIFORM2I)                               \
	GLE(Uniform3i, UNIFORM3I)                               \
	GLE(Uniform4i, UNIFORM4I)                               \
	GLE(Uniform1fv, UNIFORM1FV)                             \
	GLE(Uniform2fv, UNIFORM2FV)                             \
	GLE(Uniform3fv, UNIFORM3FV)                             \
	GLE(Uniform4fv, UNIFORM4FV)                             \
	GLE(Uniform1iv, UNIFORM1IV)                             \
	GLE(Uniform2iv, UNIFORM2IV)                             \
	GLE(Uniform3iv, UNIFORM3IV)                             \
	GLE(Uniform4iv, UNIFORM4IV)                             \
	GLE(UniformMatrix2fv, UNIFORMMATRIX2FV)                 \
	GLE(UniformMatrix3fv, UNIFORMMATRIX3FV)                 \
	GLE(UniformMatrix4fv, UNIFORMMATRIX4FV)                 \
	/* end */

#define GLE(name, caps_name) extern PFNGL##caps_name##PROC name;
GL_LIST_1
#undef GLE

#ifndef __EMSCRIPTEN__

#define GLE(name, caps_name) extern PFNGL##caps_name##PROC name;
GL_LIST_2
GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name) extern PFNGL##caps_name##OESPROC name;
GL_LIST_2
#undef GLE

#define GLE(name, caps_name) extern PFNGL##caps_name##EXTPROC name;
GL_LIST_3
#undef GLE

#endif

#endif

namespace ptgn::impl::gl {

void LoadGLFunctions();

} // namespace ptgn::impl::gl

#ifdef PTGN_DEBUG

namespace ptgn::impl::gl {

inline void ClearErrors() {
	while (glGetError() != GL_NO_ERROR) { /* glGetError clears the error queue */
	}
}

std::string_view GetErrorString(GLenum error);

void HandleErrors(std::source_location location = std::source_location::current());

} // namespace ptgn::impl::gl

#define GLCall(x)                    \
	::ptgn::impl::gl::ClearErrors(); \
	x;                               \
	::ptgn::impl::gl::HandleErrors()

#define GLCallReturn(x)                   \
	std::invoke([&]() {                   \
		::ptgn::impl::gl::ClearErrors();  \
		auto value = x;                   \
		::ptgn::impl::gl::HandleErrors(); \
		return value;                     \
	})

#else

#define GLCall(x)		x
#define GLCallReturn(x) x

#endif