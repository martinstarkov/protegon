#pragma once

#include "utility/platform.h"

// IMPORTANT: This file is not meant to be included outside the protegon library
// so keep it in .cpp files only!

namespace ptgn::gl {

#ifdef __EMSCRIPTEN__

#include "SDL_opengles2.h"

typedef void(GL_APIENTRYP PFNGLVERTEXATTRIBIPOINTERPROC)(
	GLuint index, GLint size, GLenum type, GLsizei stride, const void* pointer
);

#else

#ifdef PTGN_PLATFORM_MACOS

#include <OpenGL/OpenGL.h>

#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#endif //! ESSENTIAL_GL_PRACTICES_SUPPORT_GL3

#else

#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#endif

#endif

#ifndef PTGN_PLATFORM_MACOS

#define GL_LIST_3                   \
	GLE(TexStorage2D, TEXSTORAGE2D) \
	/* end */

#define GL_LIST_2                               \
	GLE(BindVertexArray, BINDVERTEXARRAY)       \
	GLE(GenVertexArrays, GENVERTEXARRAYS)       \
	GLE(DeleteVertexArrays, DELETEVERTEXARRAYS) \
	/* end */

#define GL_LIST_1                                           \
	GLE(AttachShader, ATTACHSHADER)                         \
	GLE(BindBuffer, BINDBUFFER)                             \
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

#ifdef __EMSCRIPTEN__

#define GLE(name, caps_name) extern PFNGL##caps_name##OESPROC name;
GL_LIST_2
#undef GLE

#define GLE(name, caps_name) extern PFNGL##caps_name##EXTPROC name;
GL_LIST_3
#undef GLE

#else

#define GLE(name, caps_name) extern PFNGL##caps_name##PROC name;
GL_LIST_2
GL_LIST_3
#undef GLE

#endif

#endif

} // namespace ptgn::gl