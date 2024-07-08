#pragma once

#include "protegon/platform.h"

// IMPORTANT: This file is not meant to be included outside the protegon library
// so keep it in .cpp files only!

namespace ptgn {

namespace gl {

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

#ifndef PTGN_PLATFORM_MACOS

#define GL_LIST                                             \
	GLE(AttachShader, ATTACHSHADER)                         \
	GLE(BindBuffer, BINDBUFFER)                             \
	GLE(BindFramebuffer, BINDFRAMEBUFFER)                   \
	GLE(BufferData, BUFFERDATA)                             \
	GLE(BufferSubData, BUFFERSUBDATA)                       \
	GLE(CheckFramebufferStatus, CHECKFRAMEBUFFERSTATUS)     \
	GLE(ClearBufferfv, CLEARBUFFERFV)                       \
	GLE(CompileShader, COMPILESHADER)                       \
	GLE(CreateProgram, CREATEPROGRAM)                       \
	GLE(CreateShader, CREATESHADER)                         \
	GLE(DeleteBuffers, DELETEBUFFERS)                       \
	GLE(DeleteFramebuffers, DELETEFRAMEBUFFERS)             \
	GLE(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY)   \
	GLE(DrawBuffers, DRAWBUFFERS)                           \
	GLE(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D)         \
	GLE(GenBuffers, GENBUFFERS)                             \
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
	GLE(GetVertexAttribdv, GETVERTEXATTRIBDV)               \
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
	GLE(VertexAttrib1d, VERTEXATTRIB1D)                     \
	GLE(VertexAttrib1dv, VERTEXATTRIB1DV)                   \
	GLE(VertexAttrib1f, VERTEXATTRIB1F)                     \
	GLE(VertexAttrib1fv, VERTEXATTRIB1FV)                   \
	GLE(VertexAttrib1s, VERTEXATTRIB1S)                     \
	GLE(VertexAttrib1sv, VERTEXATTRIB1SV)                   \
	GLE(VertexAttrib2d, VERTEXATTRIB2D)                     \
	GLE(VertexAttrib2dv, VERTEXATTRIB2DV)                   \
	GLE(VertexAttrib2f, VERTEXATTRIB2F)                     \
	GLE(VertexAttrib2fv, VERTEXATTRIB2FV)                   \
	GLE(VertexAttrib2s, VERTEXATTRIB2S)                     \
	GLE(VertexAttrib2sv, VERTEXATTRIB2SV)                   \
	GLE(VertexAttrib3d, VERTEXATTRIB3D)                     \
	GLE(VertexAttrib3dv, VERTEXATTRIB3DV)                   \
	GLE(VertexAttrib3f, VERTEXATTRIB3F)                     \
	GLE(VertexAttrib3fv, VERTEXATTRIB3FV)                   \
	GLE(VertexAttrib3s, VERTEXATTRIB3S)                     \
	GLE(VertexAttrib3sv, VERTEXATTRIB3SV)                   \
	GLE(VertexAttrib4Nbv, VERTEXATTRIB4NBV)                 \
	GLE(VertexAttrib4Niv, VERTEXATTRIB4NIV)                 \
	GLE(VertexAttrib4Nsv, VERTEXATTRIB4NSV)                 \
	GLE(VertexAttrib4Nub, VERTEXATTRIB4NUB)                 \
	GLE(VertexAttrib4Nubv, VERTEXATTRIB4NUBV)               \
	GLE(VertexAttrib4Nuiv, VERTEXATTRIB4NUIV)               \
	GLE(VertexAttrib4Nusv, VERTEXATTRIB4NUSV)               \
	GLE(VertexAttrib4bv, VERTEXATTRIB4BV)                   \
	GLE(VertexAttrib4d, VERTEXATTRIB4D)                     \
	GLE(VertexAttrib4dv, VERTEXATTRIB4DV)                   \
	GLE(VertexAttrib4f, VERTEXATTRIB4F)                     \
	GLE(VertexAttrib4fv, VERTEXATTRIB4FV)                   \
	GLE(VertexAttrib4iv, VERTEXATTRIB4IV)                   \
	GLE(VertexAttrib4s, VERTEXATTRIB4S)                     \
	GLE(VertexAttrib4sv, VERTEXATTRIB4SV)                   \
	GLE(VertexAttrib4ubv, VERTEXATTRIB4UBV)                 \
	GLE(VertexAttrib4uiv, VERTEXATTRIB4UIV)                 \
	GLE(VertexAttrib4usv, VERTEXATTRIB4USV)                 \
	GLE(VertexAttribPointer, VERTEXATTRIBPOINTER)           \
	GLE(GenVertexArrays, GENVERTEXARRAYS)                   \
	GLE(BindVertexArray, BINDVERTEXARRAY)                   \
	GLE(DeleteVertexArrays, DELETEVERTEXARRAYS)             \
	GLE(ActiveTexture, ACTIVETEXTURE)                       \
	/* end */

#define GLE(name, caps_name) extern PFNGL##caps_name##PROC name;
GL_LIST
#undef GLE

#endif

} // namespace gl

} // namespace ptgn