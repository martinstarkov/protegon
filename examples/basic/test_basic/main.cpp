// engine/src/main.cpp
#define SDL_MAIN_USE_CALLBACKS

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#ifdef __EMSCRIPTEN__
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <SDL3_image/SDL_image.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <thread>

namespace {

constexpr int kWindowStartWidth	 = 400;
constexpr int kWindowStartHeight = 400;

SDL_AppResult Fail(const char* what) {
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s: %s", what, SDL_GetError());
	return SDL_APP_FAILURE;
}

std::filesystem::path GetBasePath() {
	const char* p = SDL_GetBasePath();
	if (!p) {
		return {};
	}
	std::filesystem::path out(p);
	return out;
}

struct WindowDeleter {
	void operator()(SDL_Window* w) const noexcept {
		if (w) {
			SDL_DestroyWindow(w);
		}
	}
};

struct SurfaceDeleter {
	void operator()(SDL_Surface* s) const noexcept {
		if (s) {
			SDL_DestroySurface(s);
		}
	}
};

struct FontDeleter {
	void operator()(TTF_Font* f) const noexcept {
		if (f) {
			TTF_CloseFont(f);
		}
	}
};

struct SdlGuard {
	explicit SdlGuard(Uint32 flags) : ok(SDL_Init(flags)) {}

	~SdlGuard() {
		if (ok) {
			SDL_Quit();
		}
	}

	SdlGuard(const SdlGuard&)			 = delete;
	SdlGuard& operator=(const SdlGuard&) = delete;
	bool ok								 = false;
};

struct TtfGuard {
	TtfGuard() : ok(TTF_Init()) {}

	~TtfGuard() {
		if (ok) {
			TTF_Quit();
		}
	}

	TtfGuard(const TtfGuard&)			 = delete;
	TtfGuard& operator=(const TtfGuard&) = delete;
	bool ok								 = false;
};

struct MixerGuard {
	MixerGuard() : ok(MIX_Init()) {}

	~MixerGuard() {
		if (ok) {
			MIX_Quit();
		}
	}

	MixerGuard(const MixerGuard&)			 = delete;
	MixerGuard& operator=(const MixerGuard&) = delete;
	bool ok									 = false;
};

struct GlTexture {
	GLuint id  = 0;
	int width  = 0;
	int height = 0;

	~GlTexture() {
		Reset();
	}

	GlTexture()							   = default;
	GlTexture(const GlTexture&)			   = delete;
	GlTexture& operator=(const GlTexture&) = delete;

	GlTexture(GlTexture&& other) noexcept {
		*this = std::move(other);
	}

	GlTexture& operator=(GlTexture&& other) noexcept {
		if (this == &other) {
			return *this;
		}
		Reset();
		id			 = other.id;
		width		 = other.width;
		height		 = other.height;
		other.id	 = 0;
		other.width	 = 0;
		other.height = 0;
		return *this;
	}

	void Reset() {
		if (id) {
			glDeleteTextures(1, &id);
			id = 0;
		}
		width  = 0;
		height = 0;
	}

	explicit operator bool() const {
		return id != 0;
	}
};

std::optional<GlTexture> CreateTextureFromSurface(SDL_Surface* surface) {
	if (!surface) {
		return std::nullopt;
	}

	SDL_Surface* rgba = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_RGBA32);
	if (!rgba) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_ConvertSurface failed: %s", SDL_GetError());
		return std::nullopt;
	}
	std::unique_ptr<SDL_Surface, SurfaceDeleter> rgba_owner(rgba);

	GlTexture tex;
	glGenTextures(1, &tex.id);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	tex.width  = rgba->w;
	tex.height = rgba->h;

	GLint prev_align = 0;
	glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_align);
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

	glTexImage2D(
		GL_TEXTURE_2D, 0, GL_RGBA, rgba->w, rgba->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba->pixels
	);

	glPixelStorei(GL_UNPACK_ALIGNMENT, prev_align);
	return tex;
}

#ifdef __EMSCRIPTEN__

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 0;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_ES

#else

constexpr auto PTGN_OPENGL_MAJOR_VERSION = 3;
constexpr auto PTGN_OPENGL_MINOR_VERSION = 3;
#define PTGN_OPENGL_CONTEXT_PROFILE SDL_GL_CONTEXT_PROFILE_CORE

#endif

#ifdef __EMSCRIPTEN__

using GLCreateShaderProc	 = GLuint (*)(GLenum);
using GLShaderSourceProc	 = void (*)(GLuint, GLsizei, const char* const*, const GLint*);
using GLCompileShaderProc	 = void (*)(GLuint);
using GLGetShaderivProc		 = void (*)(GLuint, GLenum, GLint*);
using GLGetShaderInfoLogProc = void (*)(GLuint, GLsizei, GLsizei*, char*);
using GLDeleteShaderProc	 = void (*)(GLuint);

using GLCreateProgramProc	  = GLuint (*)(void);
using GLAttachShaderProc	  = void (*)(GLuint, GLuint);
using GLLinkProgramProc		  = void (*)(GLuint);
using GLGetProgramivProc	  = void (*)(GLuint, GLenum, GLint*);
using GLGetProgramInfoLogProc = void (*)(GLuint, GLsizei, GLsizei*, char*);
using GLUseProgramProc		  = void (*)(GLuint);
using GLDeleteProgramProc	  = void (*)(GLuint);

using GLGetUniformLocationProc = GLint (*)(GLuint, const char*);
using GLGetAttribLocationProc  = GLint (*)(GLuint, const char*);

using GLGenBuffersProc	  = void (*)(GLsizei, GLuint*);
using GLDeleteBuffersProc = void (*)(GLsizei, const GLuint*);
using GLBindBufferProc	  = void (*)(GLenum, GLuint);
using GLBufferDataProc	  = void (*)(GLenum, std::intptr_t, const void*, GLenum);

using GLEnableVertexAttribArrayProc	 = void (*)(GLuint);
using GLDisableVertexAttribArrayProc = void (*)(GLuint);
using GLVertexAttribPointerProc = void (*)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*);
using GLDrawArraysProc			= void (*)(GLenum, GLint, GLsizei);

using GLUniform2fProc	  = void (*)(GLint, GLfloat, GLfloat);
using GLUniform1iProc	  = void (*)(GLint, GLint);
using GLActiveTextureProc = void (*)(GLenum);

struct Gles2 {
	GLCreateShaderProc glCreateShader		  = nullptr;
	GLShaderSourceProc glShaderSource		  = nullptr;
	GLCompileShaderProc glCompileShader		  = nullptr;
	GLGetShaderivProc glGetShaderiv			  = nullptr;
	GLGetShaderInfoLogProc glGetShaderInfoLog = nullptr;
	GLDeleteShaderProc glDeleteShader		  = nullptr;

	GLCreateProgramProc glCreateProgram			= nullptr;
	GLAttachShaderProc glAttachShader			= nullptr;
	GLLinkProgramProc glLinkProgram				= nullptr;
	GLGetProgramivProc glGetProgramiv			= nullptr;
	GLGetProgramInfoLogProc glGetProgramInfoLog = nullptr;
	GLUseProgramProc glUseProgram				= nullptr;
	GLDeleteProgramProc glDeleteProgram			= nullptr;

	GLGetUniformLocationProc glGetUniformLocation = nullptr;
	GLGetAttribLocationProc glGetAttribLocation	  = nullptr;

	GLGenBuffersProc glGenBuffers		= nullptr;
	GLDeleteBuffersProc glDeleteBuffers = nullptr;
	GLBindBufferProc glBindBuffer		= nullptr;
	GLBufferDataProc glBufferData		= nullptr;

	GLEnableVertexAttribArrayProc glEnableVertexAttribArray	  = nullptr;
	GLDisableVertexAttribArrayProc glDisableVertexAttribArray = nullptr;
	GLVertexAttribPointerProc glVertexAttribPointer			  = nullptr;
	GLDrawArraysProc glDrawArrays							  = nullptr;

	GLUniform2fProc glUniform2f			= nullptr;
	GLUniform1iProc glUniform1i			= nullptr;
	GLActiveTextureProc glActiveTexture = nullptr;

	bool Load() {
#define LOAD(name)                                                                              \
	do {                                                                                        \
		name = reinterpret_cast<decltype(name)>(SDL_GL_GetProcAddress(#name));                  \
		if (!name) {                                                                            \
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load GL function %s", #name); \
			return false;                                                                       \
		}                                                                                       \
	} while (0)

		LOAD(glCreateShader);
		LOAD(glShaderSource);
		LOAD(glCompileShader);
		LOAD(glGetShaderiv);
		LOAD(glGetShaderInfoLog);
		LOAD(glDeleteShader);

		LOAD(glCreateProgram);
		LOAD(glAttachShader);
		LOAD(glLinkProgram);
		LOAD(glGetProgramiv);
		LOAD(glGetProgramInfoLog);
		LOAD(glUseProgram);
		LOAD(glDeleteProgram);

		LOAD(glGetUniformLocation);
		LOAD(glGetAttribLocation);

		LOAD(glGenBuffers);
		LOAD(glDeleteBuffers);
		LOAD(glBindBuffer);
		LOAD(glBufferData);

		LOAD(glEnableVertexAttribArray);
		LOAD(glDisableVertexAttribArray);
		LOAD(glVertexAttribPointer);
		LOAD(glDrawArrays);

		LOAD(glUniform2f);
		LOAD(glUniform1i);
		LOAD(glActiveTexture);

#undef LOAD
		return true;
	}
};

GLuint CompileShader(const Gles2& gl, GLenum type, const char* source) {
	GLuint shader = gl.glCreateShader(type);
	if (!shader) {
		return 0;
	}

	gl.glShaderSource(shader, 1, &source, nullptr);
	gl.glCompileShader(shader);

	GLint ok = GL_FALSE;
	gl.glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
	if (ok) {
		return shader;
	}

	GLint log_len = 0;
	gl.glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_len);
	std::string log(static_cast<size_t>(log_len), '\0');
	GLsizei written = 0;
	if (log_len > 0) {
		gl.glGetShaderInfoLog(shader, log_len, &written, log.data());
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Shader compile failed: %s", log.c_str());

	gl.glDeleteShader(shader);
	return 0;
}

GLuint CreateProgramTexturedQuad(const Gles2& gl) {
	static const char* kVs = R"(
#ifdef GL_ES
precision mediump float;
#endif
attribute vec2 aPos;
attribute vec2 aUV;
varying vec2 vUV;
uniform vec2 uResolution;
void main() {
  vec2 zeroToOne = aPos / uResolution;
  vec2 clip = zeroToOne * 2.0 - 1.0;
  clip.y = -clip.y;
  gl_Position = vec4(clip, 0.0, 1.0);
  vUV = aUV;
}
)";

	static const char* kFs = R"(
#ifdef GL_ES
precision mediump float;
#endif
varying vec2 vUV;
uniform sampler2D uTexture;
void main() {
  gl_FragColor = texture2D(uTexture, vUV);
}
)";

	GLuint vs = CompileShader(gl, GL_VERTEX_SHADER, kVs);
	if (!vs) {
		return 0;
	}
	GLuint fs = CompileShader(gl, GL_FRAGMENT_SHADER, kFs);
	if (!fs) {
		gl.glDeleteShader(vs);
		return 0;
	}

	GLuint prog = gl.glCreateProgram();
	if (!prog) {
		gl.glDeleteShader(vs);
		gl.glDeleteShader(fs);
		return 0;
	}

	gl.glAttachShader(prog, vs);
	gl.glAttachShader(prog, fs);
	gl.glLinkProgram(prog);

	gl.glDeleteShader(vs);
	gl.glDeleteShader(fs);

	GLint ok = GL_FALSE;
	gl.glGetProgramiv(prog, GL_LINK_STATUS, &ok);
	if (ok) {
		return prog;
	}

	GLint log_len = 0;
	gl.glGetProgramiv(prog, GL_INFO_LOG_LENGTH, &log_len);
	std::string log(static_cast<size_t>(log_len), '\0');
	GLsizei written = 0;
	if (log_len > 0) {
		gl.glGetProgramInfoLog(prog, log_len, &written, log.data());
	}
	SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Program link failed: %s", log.c_str());

	gl.glDeleteProgram(prog);
	return 0;
}

#endif // __EMSCRIPTEN__

struct GlContext {
	SDL_GLContext ctx = nullptr;

#ifdef __EMSCRIPTEN__
	Gles2 gles{};
	GLuint program	   = 0;
	GLuint vbo		   = 0;
	GLint u_resolution = -1;
	GLint u_texture	   = -1;
	GLint a_pos		   = -1;
	GLint a_uv		   = -1;
#endif

	bool Init(SDL_Window* window) {
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, PTGN_OPENGL_CONTEXT_PROFILE);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, PTGN_OPENGL_MAJOR_VERSION);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, PTGN_OPENGL_MINOR_VERSION);
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);

		ctx = SDL_GL_CreateContext(window);
		if (!ctx) {
			return false;
		}
		if (!SDL_GL_MakeCurrent(window, ctx)) {
			return false;
		}

		SDL_GL_SetSwapInterval(1);

		int w = 0, h = 0;
		SDL_GetWindowSizeInPixels(window, &w, &h);
		glViewport(0, 0, w, h);

#ifndef __EMSCRIPTEN__
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0, w, h, 0, -1, 1);
		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glEnable(GL_TEXTURE_2D);
#else
		if (!gles.Load()) {
			return false;
		}

		program = CreateProgramTexturedQuad(gles);
		if (!program) {
			return false;
		}

		gles.glUseProgram(program);
		u_resolution = gles.glGetUniformLocation(program, "uResolution");
		u_texture	 = gles.glGetUniformLocation(program, "uTexture");
		a_pos		 = gles.glGetAttribLocation(program, "aPos");
		a_uv		 = gles.glGetAttribLocation(program, "aUV");
		if (u_resolution < 0 || u_texture < 0 || a_pos < 0 || a_uv < 0) {
			return false;
		}

		gles.glGenBuffers(1, &vbo);
		if (!vbo) {
			return false;
		}

		gles.glActiveTexture(GL_TEXTURE0);
		gles.glUniform1i(u_texture, 0);
		glDisable(GL_DEPTH_TEST);
#endif

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		return true;
	}

	void Shutdown(SDL_Window* window) {
#ifdef __EMSCRIPTEN__
		if (vbo) {
			gles.glDeleteBuffers(1, &vbo);
			vbo = 0;
		}
		if (program) {
			gles.glDeleteProgram(program);
			program = 0;
		}
#endif
		if (ctx) {
			SDL_GL_MakeCurrent(window, nullptr);
			SDL_GL_DestroyContext(ctx);
			ctx = nullptr;
		}
	}

	GlContext()							   = default;
	GlContext(const GlContext&)			   = delete;
	GlContext& operator=(const GlContext&) = delete;
};

void BeginFrame(GlContext& glctx, SDL_Window* window, float r, float g, float b) {
	int w = 0, h = 0;
	SDL_GetWindowSizeInPixels(window, &w, &h);
	glViewport(0, 0, w, h);

#ifndef __EMSCRIPTEN__
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, w, h, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
#else
	glctx.gles.glUseProgram(glctx.program);
	glctx.gles.glUniform2f(glctx.u_resolution, static_cast<float>(w), static_cast<float>(h));
#endif

	glClearColor(r, g, b, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT);
}

void DrawTexture(GlContext& glctx, const GlTexture& tex, float x, float y, float w, float h) {
	if (!tex) {
		return;
	}

#ifdef __EMSCRIPTEN__
	struct Vertex {
		float x, y, u, v;
	};

	Vertex verts[6] = {
		{ x, y, 0.f, 0.f }, { x + w, y, 1.f, 0.f },		{ x + w, y + h, 1.f, 1.f },
		{ x, y, 0.f, 0.f }, { x + w, y + h, 1.f, 1.f }, { x, y + h, 0.f, 1.f },
	};

	glctx.gles.glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, tex.id);

	glctx.gles.glBindBuffer(GL_ARRAY_BUFFER, glctx.vbo);
	glctx.gles.glBufferData(
		GL_ARRAY_BUFFER, static_cast<std::intptr_t>(sizeof(verts)), verts, GL_DYNAMIC_DRAW
	);

	glctx.gles.glEnableVertexAttribArray(glctx.a_pos);
	glctx.gles.glVertexAttribPointer(
		glctx.a_pos, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<const void*>(0)
	);

	glctx.gles.glEnableVertexAttribArray(glctx.a_uv);
	glctx.gles.glVertexAttribPointer(
		glctx.a_uv, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
		reinterpret_cast<const void*>(2 * sizeof(float))
	);

	glctx.gles.glDrawArrays(GL_TRIANGLES, 0, 6);

	glctx.gles.glDisableVertexAttribArray(glctx.a_pos);
	glctx.gles.glDisableVertexAttribArray(glctx.a_uv);
#else
	glBindTexture(GL_TEXTURE_2D, tex.id);

	glBegin(GL_TRIANGLES);
	glTexCoord2f(0.f, 0.f);
	glVertex2f(x, y);
	glTexCoord2f(1.f, 0.f);
	glVertex2f(x + w, y);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(x + w, y + h);

	glTexCoord2f(0.f, 0.f);
	glVertex2f(x, y);
	glTexCoord2f(1.f, 1.f);
	glVertex2f(x + w, y + h);
	glTexCoord2f(0.f, 1.f);
	glVertex2f(x, y + h);
	glEnd();
#endif
}

void EndFrame(SDL_Window* window) {
	SDL_GL_SwapWindow(window);
}

class App {
public:
	SDL_AppResult Init(int /*argc*/, char** /*argv*/) {
		sdl_ = std::make_unique<SdlGuard>(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
		if (!sdl_->ok) {
			return Fail("SDL_Init");
		}

		ttf_ = std::make_unique<TtfGuard>();
		if (!ttf_->ok) {
			return Fail("TTF_Init");
		}

		mix_ = std::make_unique<MixerGuard>();
		if (!mix_->ok) {
			return Fail("MIX_Init");
		}

		window_.reset(SDL_CreateWindow(
			"Protegon (SDL3 + OpenGL)", kWindowStartWidth, kWindowStartHeight,
			SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY | SDL_WINDOW_OPENGL
		));
		if (!window_) {
			return Fail("SDL_CreateWindow");
		}

		if (!gl_.Init(window_.get())) {
			return Fail("GL Init");
		}

#if __ANDROID__
		base_path_ = std::filesystem::path("assets");
#else
		base_path_ = GetBasePath();
		if (base_path_.empty()) {
			return Fail("SDL_GetBasePath");
		}
#endif

		if (!LoadAssets()) {
			return SDL_APP_FAILURE;
		}
		if (!StartAudio()) {
			return SDL_APP_FAILURE;
		}

		SDL_ShowWindow(window_.get());
		return SDL_APP_CONTINUE;
	}

	SDL_AppResult HandleEvent(const SDL_Event& e) {
		if (e.type == SDL_EVENT_QUIT) {
			quit_ = SDL_APP_SUCCESS;
		}
		return SDL_APP_CONTINUE;
	}

	SDL_AppResult Tick() {
		const float t = SDL_GetTicks() / 1000.0f;
		const float r = (std::sin(t) + 1.0f) * 0.5f;
		const float g = (std::sin(t / 2.0f) + 1.0f) * 0.5f;
		const float b = (std::sin(t * 2.0f) + 1.0f) * 0.5f;

		BeginFrame(gl_, window_.get(), r, g, b);

		int win_w = 0, win_h = 0;
		SDL_GetWindowSizeInPixels(window_.get(), &win_w, &win_h);

		DrawTexture(
			gl_, image_tex_, 0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h)
		);
		DrawTexture(
			gl_, message_tex_, message_rect_.x, message_rect_.y, message_rect_.w, message_rect_.h
		);

		EndFrame(window_.get());
		return quit_;
	}

	void Shutdown(SDL_AppResult /*result*/) {
		StopAudio();

		image_tex_.Reset();
		message_tex_.Reset();

		if (window_) {
			gl_.Shutdown(window_.get());
			window_.reset();
		}

		mix_.reset();
		ttf_.reset();
		sdl_.reset();
	}

private:
	bool LoadAssets() {
		const std::filesystem::path font_path = base_path_ / "assets/Inter-VariableFont.ttf";
		std::unique_ptr<TTF_Font, FontDeleter> font(TTF_OpenFont(font_path.string().c_str(), 36));
		if (!font) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "TTF_OpenFont failed: %s", SDL_GetError());
			return false;
		}

		constexpr std::string_view kText = "Hello Basic!";
		SDL_Color white{ 255, 255, 255, 255 };

		SDL_Surface* msg = TTF_RenderText_Solid(font.get(), kText.data(), kText.size(), white);
		if (!msg) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "TTF_RenderText_Solid failed: %s", SDL_GetError()
			);
			return false;
		}
		std::unique_ptr<SDL_Surface, SurfaceDeleter> msg_owner(msg);

		auto msg_tex = CreateTextureFromSurface(msg);
		if (!msg_tex) {
			return false;
		}
		message_tex_ = std::move(*msg_tex);

		message_rect_ = SDL_FRect{
			.x = 0.0f,
			.y = 0.0f,
			.w = static_cast<float>(message_tex_.width),
			.h = static_cast<float>(message_tex_.height),
		};

		SDL_Surface* img = IMG_Load((base_path_ / "assets/logo.png").string().c_str());
		if (!img) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "IMG_Load failed: %s", SDL_GetError());
			return false;
		}
		std::unique_ptr<SDL_Surface, SurfaceDeleter> img_owner(img);

		auto img_tex = CreateTextureFromSurface(img);
		if (!img_tex) {
			return false;
		}
		image_tex_ = std::move(*img_tex);

		return true;
	}

	bool StartAudio() {
		mixer_ = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (!mixer_) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "MIX_CreateMixerDevice failed: %s", SDL_GetError()
			);
			return false;
		}

		track_ = MIX_CreateTrack(mixer_);
		if (!track_) {
			SDL_LogError(
				SDL_LOG_CATEGORY_APPLICATION, "MIX_CreateTrack failed: %s", SDL_GetError()
			);
			return false;
		}

		const std::filesystem::path music_path = base_path_ / "assets/the_entertainer.ogg";
		MIX_Audio* music = MIX_LoadAudio(mixer_, music_path.string().c_str(), false);
		if (!music) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "MIX_LoadAudio failed: %s", SDL_GetError());
			return false;
		}

		MIX_SetTrackAudio(track_, music);
		SDL_PropertiesID props = SDL_CreateProperties();
		SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
		MIX_PlayTrack(track_, props);
		return true;
	}

	void StopAudio() {
		if (!track_) {
			return;
		}
		MIX_StopTrack(track_, MIX_TrackMSToFrames(track_, 1000));
		std::this_thread::sleep_for(std::chrono::milliseconds(10));
		track_ = nullptr;
	}

private:
	std::unique_ptr<SdlGuard> sdl_;
	std::unique_ptr<TtfGuard> ttf_;
	std::unique_ptr<MixerGuard> mix_;

	std::unique_ptr<SDL_Window, WindowDeleter> window_;
	GlContext gl_;

	std::filesystem::path base_path_;

	GlTexture message_tex_;
	GlTexture image_tex_;
	SDL_FRect message_rect_{};

	MIX_Mixer* mixer_ = nullptr;
	MIX_Track* track_ = nullptr;

	SDL_AppResult quit_ = SDL_APP_CONTINUE;
};

} // namespace

SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[]) {
	auto app		= std::make_unique<App>();
	SDL_AppResult r = app->Init(argc, argv);
	if (r != SDL_APP_CONTINUE) {
		return r;
	}
	*appstate = app.release();
	return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* event) {
	auto* app = static_cast<App*>(appstate);
	return app->HandleEvent(*event);
}

SDL_AppResult SDL_AppIterate(void* appstate) {
	auto* app = static_cast<App*>(appstate);
	return app->Tick();
}

void SDL_AppQuit(void* appstate, SDL_AppResult result) {
	auto* app = static_cast<App*>(appstate);
	if (!app) {
		return;
	}
	app->Shutdown(result);
	delete app;
}
