#include "gl_loader.h"

#include <set>
#include <vector>
#include <iostream>
#include <string>
#include <fstream>
#include <string>
#include <streambuf>

//#if defined(__linux__)
//#define GLDECL // Empty define
//#define PROTEGON_GL_LIST_WIN32 // Empty define
//#endif // __linux__

//#if defined(_WIN32)
//#define NOMINMAX
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
//#define GLDECL WINAPI

// From: https://registry.khronos.org/OpenGL/api/GL/glext.h
//#define GL_ARRAY_BUFFER                   0x8892 
//#define GL_ARRAY_BUFFER_BINDING           0x8894
//#define GL_COLOR_ATTACHMENT0              0x8CE0
//#define GL_COMPILE_STATUS                 0x8B81
//#define GL_CURRENT_PROGRAM                0x8B8D
//#define GL_DYNAMIC_DRAW                   0x88E8
//#define GL_ELEMENT_ARRAY_BUFFER           0x8893
//#define GL_ELEMENT_ARRAY_BUFFER_BINDING   0x8895
//#define GL_FRAGMENT_SHADER                0x8B30
//#define GL_FRAMEBUFFER                    0x8D40
//#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
//#define GL_FUNC_ADD                       0x8006
//#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
//#define GL_MAJOR_VERSION                  0x821B
//#define GL_MINOR_VERSION                  0x821C
//#define GL_STATIC_DRAW                    0x88E4
//#define GL_STREAM_DRAW                    0x88E0
//#define GL_TEXTURE0                       0x84C0
//#define GL_VERTEX_SHADER                  0x8B31

//typedef char GLchar;
//typedef ptrdiff_t GLintptr;
//typedef ptrdiff_t GLsizeiptr;

//#define GL_LIST_WIN32 \
//    /* ret, name, params */ \
//    GLE(void,      BlendEquation,           GLenum mode) \
//    GLE(void,      ActiveTexture,           GLenum texture) \
//    /* end */
//
//#endif // _WIN32

//#include <GL/gl.h>

//#define GL_LIST \
//    /* ret, name, params */ \
//    GLE(void,    AttachShader,                GLuint program, GLuint shader) \
//    GLE(void,    BindBuffer,                  GLenum target, GLuint buffer) \
//    GLE(void,    BindFramebuffer,             GLenum target, GLuint framebuffer) \
//    GLE(void,    BufferData,                  GLenum target, GLsizeiptr size, const GLvoid *data, GLenum usage) \
//    GLE(void,    BufferSubData,               GLenum target, GLintptr offset, GLsizeiptr size, const GLvoid * data) \
//    GLE(GLenum,  CheckFramebufferStatus,      GLenum target) \
//    GLE(void,    ClearBufferfv,               GLenum buffer, GLint drawbuffer, const GLfloat * value) \
//    GLE(void,    CompileShader,               GLuint shader) \
//    GLE(GLuint,  CreateProgram,               void) \
//    GLE(GLuint,  CreateShader,                GLenum type) \
//    GLE(void,    DeleteBuffers,               GLsizei n, const GLuint *buffers) \
//    GLE(void,    DeleteFramebuffers,          GLsizei n, const GLuint *framebuffers) \
//    GLE(void,    EnableVertexAttribArray,     GLuint index) \
//    GLE(void,    DrawBuffers,                 GLsizei n, const GLenum *bufs) \
//    GLE(void,    FramebufferTexture2D,        GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) \
//    GLE(void,    GenBuffers,                  GLsizei n, GLuint *buffers) \
//    GLE(void,    GenFramebuffers,             GLsizei n, GLuint * framebuffers) \
//    GLE(GLint,   GetAttribLocation,           GLuint program, const GLchar *name) \
//    GLE(void,    GetShaderInfoLog,            GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
//    GLE(void,	   GetProgramInfoLog,           GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog) \
//    GLE(void,    GetShaderiv,                 GLuint shader, GLenum pname, GLint *params) \
//    GLE(void,    GetProgramiv,                GLuint program, GLenum pname, GLint* params) \
//    GLE(void,    DeleteShader,                GLuint shader) \
//    GLE(GLint,   GetUniformLocation,          GLuint program, const GLchar *name) \
//    GLE(void,    LinkProgram,                 GLuint program) \
//    GLE(void,    ValidateProgram,             GLuint program) \
//    GLE(void,    DeleteProgram,               GLuint program) \
//	GLE(void,	   ShaderSource,                GLuint shader, GLsizei count, const GLchar* const* string, const GLint* length) \
//    GLE(void,    UseProgram,                  GLuint program) \
//	GLE(void,	   BlendEquationSeparate,       GLenum modeRGB, GLenum modeAlpha) \
//	GLE(void,	   StencilOpSeparate,           GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) \
//	GLE(void,	   StencilFuncSeparate,         GLenum face, GLenum func, GLint ref, GLuint mask) \
//	GLE(void,	   StencilMaskSeparate,         GLenum face, GLuint mask) \
//	GLE(void,	   BindAttribLocation,          GLuint program, GLuint index, const GLchar* name) \
//	GLE(void,	   DetachShader,                GLuint program, GLuint shader) \
//	GLE(void,	   DisableVertexAttribArray,    GLuint index) \
//	GLE(void,	   GetActiveAttrib,             GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
//	GLE(void,	   GetActiveUniform,            GLuint program, GLuint index, GLsizei bufSize, GLsizei* length, GLint* size, GLenum* type, GLchar* name) \
//	GLE(void,	   GetAttachedShaders,          GLuint program, GLsizei maxCount, GLsizei* count, GLuint* shaders) \
//	GLE(void,	   GetUniformfv,                GLuint program, GLint location, GLfloat* params) \
//	GLE(void,	   GetUniformiv,                GLuint program, GLint location, GLint* params) \
//	GLE(void,	   GetVertexAttribdv,           GLuint index, GLenum pname, GLdouble* params) \
//	GLE(void,	   GetVertexAttribfv,           GLuint index, GLenum pname, GLfloat* params) \
//	GLE(void,	   GetVertexAttribiv,           GLuint index, GLenum pname, GLint* params) \
//	GLE(void,	   GetVertexAttribPointerv,     GLuint index, GLenum pname, void** pointer) \
//	GLE(GLboolean, IsProgram,                   GLuint program) \
//	GLE(GLboolean, IsShader,                    GLuint shader) \
//	GLE(void,	   Uniform1f,                   GLint location, GLfloat v0) \
//	GLE(void,	   Uniform2f,                   GLint location, GLfloat v0, GLfloat v1) \
//	GLE(void,	   Uniform3f,                   GLint location, GLfloat v0, GLfloat v1, GLfloat v2) \
//	GLE(void,	   Uniform4f,                   GLint location, GLfloat v0, GLfloat v1, GLfloat v2, GLfloat v3) \
//	GLE(void,	   Uniform1i,                   GLint location, GLint v0) \
//	GLE(void,	   Uniform2i,                   GLint location, GLint v0, GLint v1) \
//	GLE(void,	   Uniform3i,                   GLint location, GLint v0, GLint v1, GLint v2) \
//	GLE(void,	   Uniform4i,                   GLint location, GLint v0, GLint v1, GLint v2, GLint v3) \
//	GLE(void,	   Uniform1fv,                  GLint location, GLsizei count, const GLfloat* value) \
//	GLE(void,	   Uniform2fv,                  GLint location, GLsizei count, const GLfloat* value) \
//	GLE(void,	   Uniform3fv,                  GLint location, GLsizei count, const GLfloat* value) \
//	GLE(void,	   Uniform4fv,                  GLint location, GLsizei count, const GLfloat* value) \
//	GLE(void,	   Uniform1iv,                  GLint location, GLsizei count, const GLint* value) \
//	GLE(void,	   Uniform2iv,                  GLint location, GLsizei count, const GLint* value) \
//	GLE(void,	   Uniform3iv,                  GLint location, GLsizei count, const GLint* value) \
//	GLE(void,	   Uniform4iv,                  GLint location, GLsizei count, const GLint* value) \
//	GLE(void,	   UniformMatrix2fv,            GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
//	GLE(void,	   UniformMatrix3fv,            GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
//	GLE(void,	   UniformMatrix4fv,            GLint location, GLsizei count, GLboolean transpose, const GLfloat* value) \
//	GLE(void,	   VertexAttrib1d,              GLuint index, GLdouble x) \
//	GLE(void,	   VertexAttrib1dv,             GLuint index, const GLdouble* v) \
//	GLE(void,	   VertexAttrib1f,              GLuint index, GLfloat x) \
//	GLE(void,	   VertexAttrib1fv,             GLuint index, const GLfloat* v) \
//	GLE(void,	   VertexAttrib1s,              GLuint index, GLshort x) \
//	GLE(void,	   VertexAttrib1sv,             GLuint index, const GLshort* v) \
//	GLE(void,	   VertexAttrib2d,              GLuint index, GLdouble x, GLdouble y) \
//	GLE(void,	   VertexAttrib2dv,             GLuint index, const GLdouble* v) \
//	GLE(void,	   VertexAttrib2f,              GLuint index, GLfloat x, GLfloat y) \
//	GLE(void,	   VertexAttrib2fv,             GLuint index, const GLfloat* v) \
//	GLE(void,	   VertexAttrib2s,              GLuint index, GLshort x, GLshort y) \
//	GLE(void,	   VertexAttrib2sv,             GLuint index, const GLshort* v) \
//	GLE(void,	   VertexAttrib3d,              GLuint index, GLdouble x, GLdouble y, GLdouble z) \
//	GLE(void,	   VertexAttrib3dv,             GLuint index, const GLdouble* v) \
//	GLE(void,	   VertexAttrib3f,              GLuint index, GLfloat x, GLfloat y, GLfloat z) \
//	GLE(void,	   VertexAttrib3fv,             GLuint index, const GLfloat* v) \
//	GLE(void,	   VertexAttrib3s,              GLuint index, GLshort x, GLshort y, GLshort z) \
//	GLE(void,	   VertexAttrib3sv,             GLuint index, const GLshort* v) \
//	GLE(void,	   VertexAttrib4Nbv,            GLuint index, const GLbyte* v) \
//	GLE(void,	   VertexAttrib4Niv,            GLuint index, const GLint* v) \
//	GLE(void,	   VertexAttrib4Nsv,            GLuint index, const GLshort* v) \
//	GLE(void,	   VertexAttrib4Nub,            GLuint index, GLubyte x, GLubyte y, GLubyte z, GLubyte w) \
//	GLE(void,	   VertexAttrib4Nubv,           GLuint index, const GLubyte* v) \
//	GLE(void,	   VertexAttrib4Nuiv,           GLuint index, const GLuint* v) \
//	GLE(void,	   VertexAttrib4Nusv,           GLuint index, const GLushort* v) \
//	GLE(void,	   VertexAttrib4bv,             GLuint index, const GLbyte* v) \
//	GLE(void,	   VertexAttrib4d,              GLuint index, GLdouble x, GLdouble y, GLdouble z, GLdouble w) \
//	GLE(void,	   VertexAttrib4dv,             GLuint index, const GLdouble* v) \
//	GLE(void,	   VertexAttrib4f,              GLuint index, GLfloat x, GLfloat y, GLfloat z, GLfloat w) \
//	GLE(void,	   VertexAttrib4fv,             GLuint index, const GLfloat* v) \
//	GLE(void,	   VertexAttrib4iv,             GLuint index, const GLint* v) \
//	GLE(void,	   VertexAttrib4s,              GLuint index, GLshort x, GLshort y, GLshort z, GLshort w) \
//	GLE(void,	   VertexAttrib4sv,             GLuint index, const GLshort* v) \
//	GLE(void,	   VertexAttrib4ubv,            GLuint index, const GLubyte* v) \
//	GLE(void,	   VertexAttrib4uiv,            GLuint index, const GLuint* v) \
//	GLE(void,	   VertexAttrib4usv,            GLuint index, const GLushort* v) \
//	GLE(void,	   VertexAttribPointer,         GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) \
//    /* end */
//
//#define GLE(ret, name, ...) typedef ret GLDECL name##proc(__VA_ARGS__); extern name##proc * gl##name;
//GL_LIST
//GL_LIST_WIN32
//#undef GLE

#include <SDL.h>
#include <SDL_image.h>

#ifdef __APPLE__

#include "CoreFoundation/CoreFoundation.h"
#include <OpenGL/OpenGL.h>

#if ESSENTIAL_GL_PRACTICES_SUPPORT_GL3
#include <OpenGL/gl3.h>
#else
#include <OpenGL/gl.h>
#endif //!ESSENTIAL_GL_PRACTICES_SUPPORT_GL3

#else

#include <SDL_opengl.h>
#include <SDL_opengl_glext.h>

#endif

#ifndef __APPLE__

#define GL_LIST \
GLE(AttachShader, ATTACHSHADER) \
GLE(BindBuffer, BINDBUFFER) \
GLE(BindFramebuffer, BINDFRAMEBUFFER) \
GLE(BufferData, BUFFERDATA) \
GLE(BufferSubData, BUFFERSUBDATA) \
GLE(CheckFramebufferStatus, CHECKFRAMEBUFFERSTATUS) \
GLE(ClearBufferfv, CLEARBUFFERFV) \
GLE(CompileShader, COMPILESHADER) \
GLE(CreateProgram, CREATEPROGRAM) \
GLE(CreateShader, CREATESHADER) \
GLE(DeleteBuffers, DELETEBUFFERS) \
GLE(DeleteFramebuffers, DELETEFRAMEBUFFERS) \
GLE(EnableVertexAttribArray, ENABLEVERTEXATTRIBARRAY) \
GLE(DrawBuffers, DRAWBUFFERS) \
GLE(FramebufferTexture2D, FRAMEBUFFERTEXTURE2D) \
GLE(GenBuffers, GENBUFFERS) \
GLE(GenFramebuffers, GENFRAMEBUFFERS) \
GLE(GetAttribLocation, GETATTRIBLOCATION) \
GLE(GetShaderInfoLog, GETSHADERINFOLOG) \
GLE(GetProgramInfoLog, GETPROGRAMINFOLOG) \
GLE(GetShaderiv, GETSHADERIV) \
GLE(GetProgramiv, GETPROGRAMIV) \
GLE(DeleteShader, DELETESHADER) \
GLE(GetUniformLocation, GETUNIFORMLOCATION) \
GLE(LinkProgram, LINKPROGRAM) \
GLE(ValidateProgram, VALIDATEPROGRAM) \
GLE(DeleteProgram, DELETEPROGRAM) \
GLE(ShaderSource, SHADERSOURCE) \
GLE(UseProgram, USEPROGRAM) \
GLE(BlendEquationSeparate, BLENDEQUATIONSEPARATE) \
GLE(StencilOpSeparate, STENCILOPSEPARATE) \
GLE(StencilFuncSeparate, STENCILFUNCSEPARATE) \
GLE(StencilMaskSeparate, STENCILMASKSEPARATE) \
GLE(BindAttribLocation, BINDATTRIBLOCATION) \
GLE(DetachShader, DETACHSHADER) \
GLE(DisableVertexAttribArray, DISABLEVERTEXATTRIBARRAY) \
GLE(GetActiveAttrib, GETACTIVEATTRIB) \
GLE(GetActiveUniform, GETACTIVEUNIFORM) \
GLE(GetAttachedShaders, GETATTACHEDSHADERS) \
GLE(GetUniformfv, GETUNIFORMFV) \
GLE(GetUniformiv, GETUNIFORMIV) \
GLE(GetVertexAttribdv, GETVERTEXATTRIBDV) \
GLE(GetVertexAttribfv, GETVERTEXATTRIBFV) \
GLE(GetVertexAttribiv, GETVERTEXATTRIBIV) \
GLE(GetVertexAttribPointerv, GETVERTEXATTRIBPOINTERV) \
GLE(IsProgram, ISPROGRAM) \
GLE(IsShader, ISSHADER) \
GLE(Uniform1f, UNIFORM1F) \
GLE(Uniform2f, UNIFORM2F) \
GLE(Uniform3f, UNIFORM3F) \
GLE(Uniform4f, UNIFORM4F) \
GLE(Uniform1i, UNIFORM1I) \
GLE(Uniform2i, UNIFORM2I) \
GLE(Uniform3i, UNIFORM3I) \
GLE(Uniform4i, UNIFORM4I) \
GLE(Uniform1fv, UNIFORM1FV) \
GLE(Uniform2fv, UNIFORM2FV) \
GLE(Uniform3fv, UNIFORM3FV) \
GLE(Uniform4fv, UNIFORM4FV) \
GLE(Uniform1iv, UNIFORM1IV) \
GLE(Uniform2iv, UNIFORM2IV) \
GLE(Uniform3iv, UNIFORM3IV) \
GLE(Uniform4iv, UNIFORM4IV) \
GLE(UniformMatrix2fv, UNIFORMMATRIX2FV) \
GLE(UniformMatrix3fv, UNIFORMMATRIX3FV) \
GLE(UniformMatrix4fv, UNIFORMMATRIX4FV) \
GLE(VertexAttrib1d, VERTEXATTRIB1D) \
GLE(VertexAttrib1dv, VERTEXATTRIB1DV) \
GLE(VertexAttrib1f, VERTEXATTRIB1F) \
GLE(VertexAttrib1fv, VERTEXATTRIB1FV) \
GLE(VertexAttrib1s, VERTEXATTRIB1S) \
GLE(VertexAttrib1sv, VERTEXATTRIB1SV) \
GLE(VertexAttrib2d, VERTEXATTRIB2D) \
GLE(VertexAttrib2dv, VERTEXATTRIB2DV) \
GLE(VertexAttrib2f, VERTEXATTRIB2F) \
GLE(VertexAttrib2fv, VERTEXATTRIB2FV) \
GLE(VertexAttrib2s, VERTEXATTRIB2S) \
GLE(VertexAttrib2sv, VERTEXATTRIB2SV) \
GLE(VertexAttrib3d, VERTEXATTRIB3D) \
GLE(VertexAttrib3dv, VERTEXATTRIB3DV) \
GLE(VertexAttrib3f, VERTEXATTRIB3F) \
GLE(VertexAttrib3fv, VERTEXATTRIB3FV) \
GLE(VertexAttrib3s, VERTEXATTRIB3S) \
GLE(VertexAttrib3sv, VERTEXATTRIB3SV) \
GLE(VertexAttrib4Nbv, VERTEXATTRIB4NBV) \
GLE(VertexAttrib4Niv, VERTEXATTRIB4NIV) \
GLE(VertexAttrib4Nsv, VERTEXATTRIB4NSV) \
GLE(VertexAttrib4Nub, VERTEXATTRIB4NUB) \
GLE(VertexAttrib4Nubv, VERTEXATTRIB4NUBV) \
GLE(VertexAttrib4Nuiv, VERTEXATTRIB4NUIV) \
GLE(VertexAttrib4Nusv, VERTEXATTRIB4NUSV) \
GLE(VertexAttrib4bv, VERTEXATTRIB4BV) \
GLE(VertexAttrib4d, VERTEXATTRIB4D) \
GLE(VertexAttrib4dv, VERTEXATTRIB4DV) \
GLE(VertexAttrib4f, VERTEXATTRIB4F) \
GLE(VertexAttrib4fv, VERTEXATTRIB4FV) \
GLE(VertexAttrib4iv, VERTEXATTRIB4IV) \
GLE(VertexAttrib4s, VERTEXATTRIB4S) \
GLE(VertexAttrib4sv, VERTEXATTRIB4SV) \
GLE(VertexAttrib4ubv, VERTEXATTRIB4UBV) \
GLE(VertexAttrib4uiv, VERTEXATTRIB4UIV) \
GLE(VertexAttrib4usv, VERTEXATTRIB4USV) \
GLE(VertexAttribPointer, VERTEXATTRIBPOINTER) \
/* end */

#define GLE(name, caps_name) PFNGL##caps_name##PROC gl##name;
GL_LIST
#undef GLE

namespace ptgn {

namespace impl {

bool GLInit() {
#define STR(x) #x
#define GLE(name, caps_name) gl##name = (PFNGL##caps_name##PROC) SDL_GL_GetProcAddress(STR(gl##name));
	GL_LIST
#undef GLE
#undef STR

#define GLE(name, caps_name) gl##name && 
return GL_LIST true;
#undef GLE
}

#endif // __APPLE__

// TODO: Check if useful:
#ifdef __APPLE__
void InitializeAppleFileSystem() {
	/*CFBundleRef mainBundle = CFBundleGetMainBundle();
	CFURLRef resourcesURL = CFBundleCopyResourcesDirectoryURL(mainBundle);
	char path[PATH_MAX];
	if (!CFURLGetFileSystemRepresentation(resourcesURL, TRUE, (UInt8*)path, PATH_MAX)) {
		std::cerr << "Couldn't get file system representation! " << std::endl;
	}
	CFRelease(resourcesURL);
	chdir(path);*/
}
#endif

//#if defined(__linux__)
//
//    void* libGL = dlopen("libGL.so", RTLD_LAZY);
//    if (!libGL) {
//        printf("ERROR: libGL.so couldn't be loaded\n");
//        return false;
//    }
//
//#define GLE(ret, name, ...)                                                    \
//            gl##name = (name##proc *) dlsym(libGL, "gl" #name);                    \
//            if (!gl##name) {                                                       \
//                printf("Function gl" #name " couldn't be loaded from libGL.so\n"); \
//                return false;                                                      \
//            }
//GL_LIST
//#undef GLE
//
//#elif defined(_WIN32)
//
//    HINSTANCE dll = LoadLibraryA("opengl32.dll");
//    typedef PROC WINAPI wglGetProcAddressproc(LPCSTR lpszProc);
//    if (!dll) {
//        OutputDebugStringA("opengl32.dll not found.\n");
//        return false;
//    }
//    wglGetProcAddressproc* wglGetProcAddress =
//        (wglGetProcAddressproc*)GetProcAddress(dll, "wglGetProcAddress");
//
//#define GLE(ret, name, ...)                                                                        \
//            gl##name = (name##proc *)SDL_GL_GetProcAddress("gl" #name);                                \
//            if (!gl##name) {                                                                       \
//                OutputDebugStringA("Function gl" #name " couldn't be loaded from opengl32.dll\n"); \
//                return false;                                                                      \
//            }
////GL_LIST
////GL_LIST_WIN32
//#undef GLE
//
//#else
//#error "GL loading for this platform is not implemented yet."
//#endif
//
//    return true;
//}

} // namespace impl

} // namespace ptgn

GLuint compileShader(const char* source, GLuint shaderType) {
	std::cout << "Compilando shader:" << std::endl << source << std::endl;
	// Create ID for shader
	GLuint result = glCreateShader(shaderType);
	// Define shader text
	glShaderSource(result, 1, &source, NULL);
	// Compile shader
	glCompileShader(result);

	//Check vertex shader for errors
	GLint shaderCompiled = GL_FALSE;
	glGetShaderiv(result, GL_COMPILE_STATUS, &shaderCompiled);
	if (shaderCompiled != GL_TRUE) {
		std::cout << "Error en la compilación: " << result << "!" << std::endl;
		GLint logLength;
		glGetShaderiv(result, GL_INFO_LOG_LENGTH, &logLength);
		if (logLength > 0) {
			GLchar* log = (GLchar*)malloc(logLength);
			glGetShaderInfoLog(result, logLength, &logLength, log);
			std::cout << "Shader compile log:" << log << std::endl;
			free(log);
		}
		glDeleteShader(result);
		result = 0;
	} else {
		std::cout << "Shader compilado correctamente. Id = " << result << std::endl;
	}
	return result;
}

GLuint compileProgram(const char* vtxFile, const char* fragFile) {
	GLuint programId = 0;
	GLuint vtxShaderId, fragShaderId;

	programId = glCreateProgram();

	std::ifstream f(vtxFile);
	std::string source((std::istreambuf_iterator<char>(f)),
		std::istreambuf_iterator<char>());
	vtxShaderId = compileShader(source.c_str(), GL_VERTEX_SHADER);

	f = std::ifstream(fragFile);
	source = std::string((std::istreambuf_iterator<char>(f)),
		std::istreambuf_iterator<char>());
	fragShaderId = compileShader(source.c_str(), GL_FRAGMENT_SHADER);

	if (vtxShaderId && fragShaderId) {
		// Associate shader with program
		glAttachShader(programId, vtxShaderId);
		glAttachShader(programId, fragShaderId);
		glLinkProgram(programId);
		glValidateProgram(programId);

		// Check the status of the compile/link
		GLint logLen;
		glGetProgramiv(programId, GL_INFO_LOG_LENGTH, &logLen);
		if (logLen > 0) {
			char* log = (char*)malloc(logLen * sizeof(char));
			// Show any errors as appropriate
			glGetProgramInfoLog(programId, logLen, &logLen, log);
			std::cout << "Prog Info Log: " << std::endl << log << std::endl;
			free(log);
		}
	}
	if (vtxShaderId) {
		glDeleteShader(vtxShaderId);
	}
	if (fragShaderId) {
		glDeleteShader(fragShaderId);
	}
	return programId;
}

void presentBackBuffer(SDL_Renderer* renderer, SDL_Window* win, SDL_Texture* backBuffer, GLuint programId, float playing_time) {
	GLint oldProgramId;
	// Guarrada para obtener el textureid (en driverdata->texture)
	//Detach the texture
	SDL_SetRenderTarget(renderer, NULL);
	SDL_RenderClear(renderer);

	SDL_GL_BindTexture(backBuffer, NULL, NULL);
	if (programId != 0) {
		glGetIntegerv(GL_CURRENT_PROGRAM, &oldProgramId);
		glUseProgram(programId);
	}

	GLfloat minx, miny, maxx, maxy;
	GLfloat minu, maxu, minv, maxv;

	// Coordenadas de la ventana donde pintar.
	minx = -1;
	miny = -1;
	maxx = 1;
	maxy = 1;

	minu = 0.0f;
	maxu = 1.0f;
	minv = 0.0f;
	maxv = 1.0f;

	glBegin(GL_TRIANGLE_STRIP);
	//glTexCoord2f(minu, minv);
	glVertex2f(minx, miny);
	//glTexCoord2f(maxu, minv);
	glVertex2f(maxx, miny);
	//glTexCoord2f(minu, maxv);
	glVertex2f(minx, maxy);
	//glTexCoord2f(maxu, maxv);
	glVertex2f(maxx, maxy);
	glEnd();

	GLint loc1 = glGetUniformLocation(programId, "iResolution");
	glUniform3f(loc1, WIN_WIDTH, WIN_HEIGHT, 0);

	GLint loc2 = glGetUniformLocation(programId, "iTime");
	glUniform1f(loc2, playing_time);

	SDL_GL_SwapWindow(win);

	if (programId != 0) {
		glUseProgram(oldProgramId);
	}

	SDL_RenderCopy(renderer, backBuffer, NULL, NULL);
}

void TestFunction() {
	GLuint programId;

#ifdef __APPLE__
	InitializeAppleFileSystem();
#endif

	if (SDL_Init(SDL_INIT_EVERYTHING | SDL_VIDEO_OPENGL) != 0) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return;
	}

	SDL_Window* win = SDL_CreateWindow("Custom shader with SDL2 renderer!", SDL_WINDOWPOS_CENTERED,
		SDL_WINDOWPOS_CENTERED, WIN_WIDTH, WIN_HEIGHT, 0);

	SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");

	SDL_Renderer* renderer = SDL_CreateRenderer(win, -1,
		SDL_RENDERER_ACCELERATED);

	SDL_RendererInfo rendererInfo;
	SDL_GetRendererInfo(renderer, &rendererInfo);

	if (!strncmp(rendererInfo.name, "opengl", 6)) {
		std::cout << "Es OpenGL!" << std::endl;
#ifndef __APPLE__
		// If you want to use GLEW or some other GL extension handler, do it here!
		if (!ptgn::impl::GLInit()) {
			std::cout << "Couldn't init GL extensions!" << std::endl;
			SDL_Quit();
			exit(-1);
		}
#endif
		// Compilar el shader y dejarlo listo para usar.
		//programId = compileProgram("resources/shader/std.vertex", "resources/shader/crt.fragment");
		programId = compileProgram("resources/shader/main_vert.glsl", "resources/shader/fire_ball_frag.glsl");
		//std::cout << "programId = " << programId << std::endl;
	}

	GLfloat iResolution[3] = { WIN_WIDTH, WIN_HEIGHT, 0 };
	clock_t start_time = clock();
	clock_t curr_time;
	float playtime_in_second = 0;

	SDL_Texture* texTarget = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
		SDL_TEXTUREACCESS_TARGET, WIN_WIDTH, WIN_HEIGHT);

	int done = 0;
	int useShader = 0;
	// Voy a poner el fondo blanco para que se vea el shader
	SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);

	while (!done) {

		curr_time = clock();
		playtime_in_second = (curr_time - start_time) * 1.0f / 1000.0f;

		presentBackBuffer(renderer, win, texTarget, programId, playtime_in_second);

		/* This could go in a separate function */
		SDL_Event event;
		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				done = 1;
			}
			if (event.type == SDL_KEYDOWN) {
				if (event.key.keysym.sym == SDLK_SPACE) {
					useShader ^= 1;
					std::cout << "useShader = " << (useShader ? "true" : "false") << std::endl;
				}
				if (event.key.keysym.sym == SDLK_ESCAPE) {
					done = 1;
				}
			}
		}
	}

	SDL_DestroyTexture(texTarget);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(win);
	SDL_Quit();
}