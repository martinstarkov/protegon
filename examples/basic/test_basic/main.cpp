#include <SDL3/SDL.h>

#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

#include "renderer/backend/gl/gl.h"

namespace ptgn {

// ----------------------------------- Math -----------------------------------
struct Vec2 {
	float x{}, y{};
};

struct Vec4 {
	float x{}, y{}, z{}, w{};
};

static Vec4 White() {
	return { 1, 1, 1, 1 };
}

// ----------------------------------- GL -------------------------------------
static GLuint CompileShader(GLenum type, const char* src) {
	GLuint s = CreateShader(type);
	ShaderSource(s, 1, &src, nullptr);
	::CompileShader(s);
	GLint ok = 0;
	GetShaderiv(s, GL_COMPILE_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		GetShaderiv(s, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		GetShaderInfoLog(s, len, nullptr, log.data());
		std::cerr << "Shader compile error:\n" << log << "\n";
		std::exit(1);
	}
	return s;
}

static GLuint LinkProgram(const char* vs, const char* fs) {
	GLuint v = CompileShader(GL_VERTEX_SHADER, vs);
	GLuint f = CompileShader(GL_FRAGMENT_SHADER, fs);
	GLuint p = CreateProgram();
	AttachShader(p, v);
	AttachShader(p, f);
	::LinkProgram(p);
	GLint ok = 0;
	GetProgramiv(p, GL_LINK_STATUS, &ok);
	if (!ok) {
		GLint len = 0;
		GetProgramiv(p, GL_INFO_LOG_LENGTH, &len);
		std::string log(len, '\0');
		GetProgramInfoLog(p, len, nullptr, log.data());
		std::cerr << "Program link error:\n" << log << "\n";
		std::exit(1);
	}
	DeleteShader(v);
	DeleteShader(f);
	return p;
}

static void SetUniform1i(GLuint prog, const char* name, int v) {
	GLint loc = GetUniformLocation(prog, name);
	if (loc >= 0) {
		Uniform1i(loc, v);
	}
}

static void SetUniform1f(GLuint prog, const char* name, float v) {
	GLint loc = GetUniformLocation(prog, name);
	if (loc >= 0) {
		Uniform1f(loc, v);
	}
}

static void SetUniform2f(GLuint prog, const char* name, Vec2 v) {
	GLint loc = GetUniformLocation(prog, name);
	if (loc >= 0) {
		Uniform2f(loc, v.x, v.y);
	}
}

// ----------------------------------- Blend ----------------------------------
enum class BlendMode {
	ReplaceRGBA,
	Alpha,
	AdditiveRGBA
};

static void ApplyBlendMode(BlendMode mode) {
	if (mode == BlendMode::ReplaceRGBA) {
		glDisable(GL_BLEND);
		return;
	}
	glEnable(GL_BLEND);
	if (mode == BlendMode::Alpha) {
		BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	} else if (mode == BlendMode::AdditiveRGBA) {
		BlendFuncSeparate(GL_ONE, GL_ONE, GL_ONE, GL_ONE);
	}
}

// ----------------------------------- Vertex ---------------------------------
struct Vertex {
	Vec2 pos;
	Vec2 uv;
	Vec4 color;
	float texIndex;
};

// ------------------------------ RenderTargetPool ----------------------------
struct TargetKey {
	int w = 0, h = 0;
	GLenum internalFormat = GL_RGBA8;

	bool operator==(const TargetKey& o) const {
		return w == o.w && h == o.h && internalFormat == o.internalFormat;
	}
};

struct TargetKeyHash {
	size_t operator()(const TargetKey& k) const noexcept {
		size_t h1 = std::hash<int>{}(k.w);
		size_t h2 = std::hash<int>{}(k.h);
		size_t h3 = std::hash<int>{}((int)k.internalFormat);
		return h1 ^ (h2 << 1) ^ (h3 << 2);
	}
};

struct RenderTarget {
	GLuint fbo = 0;
	GLuint tex = 0;
	int w = 0, h = 0;
	GLenum internalFormat = GL_RGBA8;

	uint64_t lastUsedFrame = 0;
	bool inUse			   = false;
};

static bool IsFloatFormat(GLenum internalFormat) {
	return internalFormat == GL_RGBA16F || internalFormat == GL_RGBA32F;
}

static RenderTarget CreateRenderTarget(int w, int h, GLenum internalFormat) {
	RenderTarget t;
	t.w				 = w;
	t.h				 = h;
	t.internalFormat = internalFormat;

	glGenTextures(1, &t.tex);
	glBindTexture(GL_TEXTURE_2D, t.tex);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	GLenum format = GL_RGBA;
	GLenum type	  = IsFloatFormat(internalFormat) ? GL_FLOAT : GL_UNSIGNED_BYTE;

	glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, w, h, 0, format, type, nullptr);

	GenFramebuffers(1, &t.fbo);
	BindFramebuffer(GL_FRAMEBUFFER, t.fbo);
	FramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, t.tex, 0);

	GLenum status = CheckFramebufferStatus(GL_FRAMEBUFFER);
	if (status != GL_FRAMEBUFFER_COMPLETE) {
		std::cerr << "FBO incomplete\n";
		std::exit(1);
	}
	BindFramebuffer(GL_FRAMEBUFFER, 0);
	return t;
}

static void DestroyRenderTarget(RenderTarget& t) {
	if (t.fbo) {
		DeleteFramebuffers(1, &t.fbo);
	}
	if (t.tex) {
		glDeleteTextures(1, &t.tex);
	}
	t = {};
}

class RenderTargetPool {
public:
	explicit RenderTargetPool(uint64_t ttlFrames = 240) : ttl(ttlFrames) {}

	RenderTarget* Acquire(const TargetKey& key, uint64_t frame) {
		auto& vec = pool[key];
		for (auto& rt : vec) {
			if (!rt.inUse) {
				rt.inUse		 = true;
				rt.lastUsedFrame = frame;
				return &rt;
			}
		}
		vec.push_back(CreateRenderTarget(key.w, key.h, key.internalFormat));
		vec.back().inUse		 = true;
		vec.back().lastUsedFrame = frame;
		return &vec.back();
	}

	void Release(RenderTarget* rt, uint64_t frame) {
		if (!rt) {
			return;
		}
		rt->inUse		  = false;
		rt->lastUsedFrame = frame;
	}

	void GarbageCollect(uint64_t frame) {
		for (auto it = pool.begin(); it != pool.end(); ++it) {
			auto& vec = it->second;
			for (auto vit = vec.begin(); vit != vec.end();) {
				if (!vit->inUse && (frame - vit->lastUsedFrame) > ttl) {
					DestroyRenderTarget(*vit);
					vit = vec.erase(vit);
				} else {
					++vit;
				}
			}
		}
	}

private:
	uint64_t ttl;
	std::unordered_map<TargetKey, std::vector<RenderTarget>, TargetKeyHash> pool;
};

// ------------------------------------ Pipes ---------------------------------
using PipeId = uint32_t;

struct Pipe {
	RenderTarget* ping = nullptr;
	RenderTarget* pong = nullptr; // LAZY
	bool flip		   = false;

	bool aliased	= false;
	GLuint aliasTex = 0;

	bool HasPong() const {
		return pong != nullptr;
	}

	GLuint ReadTex() const {
		if (aliased) {
			return aliasTex;
		}
		// If pong doesn't exist, always read ping
		if (!pong) {
			return ping->tex;
		}
		return flip ? pong->tex : ping->tex;
	}

	GLuint WriteFbo() const {
		// If pong doesn't exist, always write ping
		if (!pong) {
			return ping->fbo;
		}
		return flip ? ping->fbo : pong->fbo;
	}

	int W() const {
		return ping->w;
	}

	int H() const {
		return ping->h;
	}

	void SwapAfterWrite() {
		aliased	 = false;
		aliasTex = 0;
		if (pong) {
			flip = !flip; // only meaningful if pong exists
		}
	}
};

// ---------------------------------- TextureRef ------------------------------
struct TextureRef {
	struct Explicit {
		GLuint tex = 0;
	};

	struct PipeTex {
		PipeId pipe = 0;
	};

	std::variant<Explicit, PipeTex> v;

	static TextureRef FromTex(GLuint t) {
		return TextureRef{ Explicit{ t } };
	}

	static TextureRef FromPipe(PipeId p) {
		return TextureRef{ PipeTex{ p } };
	}
};

// ---------------------------------- State -----------------------------------
struct StateSnapshot {
	GLuint framebuffer = 0;
	GLuint shader	   = 0;
	BlendMode blend	   = BlendMode::ReplaceRGBA;
	int vpW = 0, vpH = 0;
	static constexpr int kMaxSlots = 16;
	GLuint texSlots[kMaxSlots]{};
};

// -------------------------------- StateChange variant -----------------------
struct SetPipeOut {
	PipeId pipe;
};

struct BindPipeIn {
	int slot;
	PipeId pipe;
};

struct SetShader {
	GLuint prog;
};

struct SetBlend {
	BlendMode mode;
};

struct SetUniform1fSC {
	const char* name;
	float v;
};

struct SetUniform2fSC {
	const char* name;
	Vec2 v;
};

struct BindTexture2D {
	int slot;
	GLuint tex;
};

using StateChange = std::variant<
	SetPipeOut, BindPipeIn, SetShader, SetBlend, SetUniform1fSC, SetUniform2fSC, BindTexture2D>;

// ----------------------------- Recorded draws -------------------------------
enum class DrawKind {
	BatchGeom,
	Immediate
};

struct RecordedClear {
	PipeId pipe = 0;
	Vec4 color{};
};

struct RecordedDraw {
	DrawKind kind = DrawKind::BatchGeom;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<StateChange> state;
	TextureRef texture = TextureRef::FromTex(0);
};

// ----------------------------- Compiled exec ops ----------------------------
static constexpr size_t kMaxBatchVerts = 30000;
static constexpr size_t kMaxBatchIdx   = 90000;
static constexpr int kMaxBatchTextures = 16;

struct BatchKey {
	GLuint framebuffer = 0;
	BlendMode blend	   = BlendMode::ReplaceRGBA;
	int vpW = 0, vpH = 0;

	bool operator==(const BatchKey& o) const {
		return framebuffer == o.framebuffer && blend == o.blend && vpW == o.vpW && vpH == o.vpH;
	}
};

struct ClearOp {
	PipeId pipe = 0;
	Vec4 color{};
};

struct DrawBatchOp {
	BatchKey key;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<GLuint> textures;
};

struct DrawImmediateOp {
	StateSnapshot state;
	std::vector<Vertex> vertices;
	std::vector<uint32_t> indices;
	std::vector<SetUniform1fSC> u1f;
	std::vector<SetUniform2fSC> u2f;
	std::optional<PipeId> swapPipeAfter;
};

using ExecOp = std::variant<ClearOp, DrawBatchOp, DrawImmediateOp>;

// --------------------------------- Mesh stream ------------------------------
struct GLMeshStream {
	GLuint vao = 0, vbo = 0, ebo = 0;

	void Init() {
		GenVertexArrays(1, &vao);
		GenBuffers(1, &vbo);
		GenBuffers(1, &ebo);
		BindVertexArray(vao);
		BindBuffer(GL_ARRAY_BUFFER, vbo);
		BindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
		EnableVertexAttribArray(0);
		VertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
		EnableVertexAttribArray(1);
		VertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
		EnableVertexAttribArray(2);
		VertexAttribPointer(
			2, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color)
		);
		EnableVertexAttribArray(3);
		VertexAttribPointer(
			3, 1, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texIndex)
		);
	}

	void Destroy() {
		DeleteBuffers(1, &ebo);
		DeleteBuffers(1, &vbo);
		DeleteVertexArrays(1, &vao);
	}
};

// ----------------------------- Encoder + Pass API ---------------------------
class RenderEncoder;

class PassBuilder {
public:
	explicit PassBuilder(RenderEncoder& e) : enc(e) {}

	PassBuilder& Out(PipeId p);
	PassBuilder& In(int slot, PipeId p);
	PassBuilder& Shader(GLuint prog);
	PassBuilder& Blend(BlendMode m);
	PassBuilder& Uniform1f(const char* name, float v);
	PassBuilder& Uniform2f(const char* name, Vec2 v);
	void Draw(std::span<const Vertex> v, std::span<const uint32_t> i);

private:
	RenderEncoder& enc;
	PipeId outPipe	= 0;
	GLuint shader	= 0;
	BlendMode blend = BlendMode::ReplaceRGBA;
	std::vector<std::pair<int, PipeId>> inputs;
	std::vector<SetUniform1fSC> u1f;
	std::vector<SetUniform2fSC> u2f;
};

class RenderEncoder {
public:
	explicit RenderEncoder(RenderTargetPool& pool) : pool(pool) {}

	void BeginFrame(uint64_t frameIndex) {
		frame = frameIndex;
	}

	PipeId CreatePipe(int w, int h, GLenum fmt, bool wantPong = false) {
		PipeId id = nextPipe++;
		TargetKey key{ w, h, fmt };
		Pipe p;
		p.ping		 = pool.Acquire(key, frame);
		p.pong		 = wantPong ? pool.Acquire(key, frame) : nullptr;
		pipes[id]	 = p;
		pipeKeys[id] = key;
		return id;
	}

	void EnsurePong(PipeId id) {
		Pipe& p = pipes.at(id);
		if (p.pong) {
			return;
		}
		TargetKey key = pipeKeys.at(id);
		p.pong		  = pool.Acquire(key, frame);
		p.flip		  = false;
	}

	PipeId Fork(PipeId src) {
		assert(pipes.count(src));
		PipeId id	  = nextPipe++;
		TargetKey key = pipeKeys.at(src);
		Pipe p;
		p.ping		 = pool.Acquire(key, frame); // only ping
		p.pong		 = nullptr;
		p.aliased	 = true;
		p.aliasTex	 = pipes.at(src).ReadTex();
		pipes[id]	 = p;
		pipeKeys[id] = key;
		return id;
	}

	void ReleasePipe(PipeId id) {
		if (!pipes.count(id)) {
			return;
		}
		Pipe& p = pipes[id];
		pool.Release(p.ping, frame);
		if (p.pong) {
			pool.Release(p.pong, frame);
		}
		pipes.erase(id);
		pipeKeys.erase(id);
	}

	void ResizePipe(PipeId id, int newW, int newH) {
		if (!pipes.count(id)) {
			return;
		}
		TargetKey oldKey = pipeKeys.at(id);
		TargetKey newKey{ newW, newH, oldKey.internalFormat };
		if (oldKey == newKey) {
			return;
		}

		Pipe& p = pipes[id];
		pool.Release(p.ping, frame);
		if (p.pong) {
			pool.Release(p.pong, frame);
		}

		p		   = Pipe{};
		p.ping	   = pool.Acquire(newKey, frame);
		p.pong	   = nullptr; // still lazy after resize
		p.flip	   = false;
		p.aliased  = false;
		p.aliasTex = 0;

		pipeKeys[id] = newKey;
	}

	Pipe& GetPipe(PipeId p) {
		return pipes.at(p);
	}

	PassBuilder Pass() {
		return PassBuilder(*this);
	}

	// Batched API
	void DrawSprite(PipeId out, Vec2 c, Vec2 hs, Vec4 col, TextureRef tex, BlendMode blend);

	void DrawRect(PipeId out, Vec2 c, Vec2 hs, Vec4 col, BlendMode blend = BlendMode::Alpha) {
		DrawSprite(out, c, hs, col, TextureRef::FromTex(0), blend);
	}

	void DrawCircle(
		PipeId out, Vec2 c, float r, Vec4 col, BlendMode blend = BlendMode::Alpha, int segments = 32
	);

	// Effects
	void DrawGrayscale(PipeId pipe, GLuint shader);
	void DrawBlur(PipeId pipe, GLuint blurH, GLuint blurV, int iterations);
	void DrawLight(
		PipeId scene, PipeId lightPipe, GLuint lightShader, int blurIterations, GLuint blurH,
		GLuint blurV
	);

	void Clear(PipeId pipe, Vec4 color);

	void Execute(GLuint batchShader, GLuint whiteTex, int screenW, int screenH);
	void Present(PipeId pipe, GLuint presentShader, int screenW, int screenH);

	GLuint ResolveTexture(const TextureRef& ref) const {
		return std::visit(
			[&](auto&& v) -> GLuint {
				using T = std::decay_t<decltype(v)>;
				if constexpr (std::is_same_v<T, TextureRef::Explicit>) {
					return v.tex;
				} else {
					return pipes.at(v.pipe).ReadTex();
				}
			},
			ref.v
		);
	}

private:
	friend class PassBuilder;

	void Record(RecordedDraw&& d) {
		recorded.emplace_back(std::move(d));
	}

	void Record(RecordedClear&& c) {
		recorded.emplace_back(std::move(c));
	}

	void ApplyStateChange(
		StateSnapshot& st, const StateChange& sc, std::vector<SetUniform1fSC>& outU1f,
		std::vector<SetUniform2fSC>& outU2f
	);

private:
	RenderTargetPool& pool;
	uint64_t frame	= 0;
	PipeId nextPipe = 1;
	std::unordered_map<PipeId, Pipe> pipes;
	std::unordered_map<PipeId, TargetKey> pipeKeys;
	using RecordedCmd = std::variant<RecordedDraw, RecordedClear>;
	std::vector<RecordedCmd> recorded;
};

// -------------------------------- PassBuilder impl --------------------------
PassBuilder& PassBuilder::Out(PipeId p) {
	outPipe = p;
	return *this;
}

PassBuilder& PassBuilder::In(int slot, PipeId p) {
	inputs.push_back({ slot, p });
	return *this;
}

PassBuilder& PassBuilder::Shader(GLuint prog) {
	shader = prog;
	return *this;
}

PassBuilder& PassBuilder::Blend(BlendMode m) {
	blend = m;
	return *this;
}

PassBuilder& PassBuilder::Uniform1f(const char* name, float v) {
	u1f.push_back({ name, v });
	return *this;
}

PassBuilder& PassBuilder::Uniform2f(const char* name, Vec2 v) {
	u2f.push_back({ name, v });
	return *this;
}

void PassBuilder::Draw(std::span<const Vertex> v, std::span<const uint32_t> i) {
	RecordedDraw d;
	d.kind = DrawKind::Immediate;
	d.vertices.assign(v.begin(), v.end());
	d.indices.assign(i.begin(), i.end());
	d.state.push_back(SetPipeOut{ outPipe });
	d.state.push_back(SetShader{ shader });
	d.state.push_back(SetBlend{ blend });
	for (auto& [slot, pipe] : inputs) {
		d.state.push_back(BindPipeIn{ slot, pipe });
	}
	for (auto& u : u1f) {
		d.state.push_back(u);
	}
	for (auto& u : u2f) {
		d.state.push_back(u);
	}
	enc.Record(std::move(d));
}

// ---------------------------- Geometry helpers ------------------------------
static void AppendQuad(
	std::vector<Vertex>& v, std::vector<uint32_t>& i, Vec2 c, Vec2 hs, Vec4 col, float texIndex
) {
	uint32_t base = (uint32_t)v.size();
	v.push_back(Vertex{ { c.x - hs.x, c.y - hs.y }, { 0, 0 }, col, texIndex });
	v.push_back(Vertex{ { c.x + hs.x, c.y - hs.y }, { 1, 0 }, col, texIndex });
	v.push_back(Vertex{ { c.x + hs.x, c.y + hs.y }, { 1, 1 }, col, texIndex });
	v.push_back(Vertex{ { c.x - hs.x, c.y + hs.y }, { 0, 1 }, col, texIndex });
	i.push_back(base + 0);
	i.push_back(base + 1);
	i.push_back(base + 2);
	i.push_back(base + 2);
	i.push_back(base + 3);
	i.push_back(base + 0);
}

static std::array<Vertex, 4> FullscreenQuadV() {
	return {
		Vertex{ { -1, -1 }, { 0, 0 }, White(), 0 },
		Vertex{ { 1, -1 }, { 1, 0 }, White(), 0 },
		Vertex{ { 1, 1 }, { 1, 1 }, White(), 0 },
		Vertex{ { -1, 1 }, { 0, 1 }, White(), 0 },
	};
}

static std::array<uint32_t, 6> FullscreenQuadI() {
	return { 0, 1, 2, 2, 3, 0 };
}

// ---------------------------- Encoder API -----------------------------------
void RenderEncoder::DrawSprite(
	PipeId out, Vec2 c, Vec2 hs, Vec4 col, TextureRef tex, BlendMode blend
) {
	RecordedDraw d;
	d.kind	  = DrawKind::BatchGeom;
	d.texture = tex;
	d.state.push_back(SetPipeOut{ out });
	d.state.push_back(SetBlend{ blend });
	AppendQuad(d.vertices, d.indices, c, hs, col, -1.0f);
	Record(std::move(d));
}

void RenderEncoder::DrawCircle(
	PipeId out, Vec2 c, float r, Vec4 col, BlendMode blend, int segments
) {
	RecordedDraw d;
	d.kind	  = DrawKind::BatchGeom;
	d.texture = TextureRef::FromTex(0);
	d.state.push_back(SetPipeOut{ out });
	d.state.push_back(SetBlend{ blend });

	d.vertices.push_back(Vertex{ { c.x, c.y }, { 0.5f, 0.5f }, col, -1.0f });
	for (int s = 0; s <= segments; s++) {
		float a = (float)s / (float)segments * 2.0f * 3.1415926f;
		d.vertices.push_back(Vertex{
			{ c.x + std::cos(a) * r, c.y + std::sin(a) * r }, { 0, 0 }, col, -1.0f });
	}
	for (int s = 1; s <= segments; s++) {
		d.indices.push_back(0);
		d.indices.push_back(s);
		d.indices.push_back(s + 1);
	}
	Record(std::move(d));
}

void RenderEncoder::DrawGrayscale(PipeId pipe, GLuint shader) {
	auto v = FullscreenQuadV();
	auto i = FullscreenQuadI();
	Pass().Out(pipe).In(0, pipe).Shader(shader).Draw(v, i);
}

void RenderEncoder::DrawBlur(PipeId pipe, GLuint blurH, GLuint blurV, int iterations) {
	auto v = FullscreenQuadV();
	auto i = FullscreenQuadI();

	// This effect reads and writes same pipe => pong required
	EnsurePong(pipe);

	Vec2 texel{ 1.0f / (float)GetPipe(pipe).W(), 1.0f / (float)GetPipe(pipe).H() };
	for (int it = 0; it < iterations; ++it) {
		Pass()
			.Out(pipe)
			.In(0, pipe)
			.Shader(blurH)
			.Uniform2f("u_TexelSize", texel)
			.Uniform2f("u_Direction", { 1, 0 })
			.Draw(v, i);
		Pass()
			.Out(pipe)
			.In(0, pipe)
			.Shader(blurV)
			.Uniform2f("u_TexelSize", texel)
			.Uniform2f("u_Direction", { 0, 1 })
			.Draw(v, i);
	}
}

void RenderEncoder::DrawLight(
	PipeId scene, PipeId lightPipe, GLuint lightShader, int blurIterations, GLuint blurH,
	GLuint blurV
) {
	auto v = FullscreenQuadV();
	auto i = FullscreenQuadI();

	Pass().Out(lightPipe).Shader(lightShader).Blend(BlendMode::ReplaceRGBA).Draw(v, i);

	if (blurIterations > 0) {
		DrawBlur(lightPipe, blurH, blurV, blurIterations);
	}

	DrawSprite(
		scene, { 0, 0 }, { 1, 1 }, White(), TextureRef::FromPipe(lightPipe), BlendMode::AdditiveRGBA
	);
}

// ----------------------------- State changes --------------------------------
void RenderEncoder::ApplyStateChange(
	StateSnapshot& st, const StateChange& sc, std::vector<SetUniform1fSC>& outU1f,
	std::vector<SetUniform2fSC>& outU2f
) {
	std::visit(
		[&](auto&& cmd) {
			using T = std::decay_t<decltype(cmd)>;
			if constexpr (std::is_same_v<T, SetPipeOut>) {
				Pipe& p		   = pipes.at(cmd.pipe);
				st.framebuffer = p.WriteFbo();
				st.vpW		   = p.W();
				st.vpH		   = p.H();
			} else if constexpr (std::is_same_v<T, BindPipeIn>) {
				// If pass reads and writes same pipe, we need pong
				// (because WriteFbo may be ping, ReadTex must be the other side).
				// We don't know output here, but safe: if binding pipe input and
				// later Out is same pipe, compilation will ensure it.
				st.texSlots[cmd.slot] = pipes.at(cmd.pipe).ReadTex();
			} else if constexpr (std::is_same_v<T, BindTexture2D>) {
				st.texSlots[cmd.slot] = cmd.tex;
			} else if constexpr (std::is_same_v<T, SetShader>) {
				st.shader = cmd.prog;
			} else if constexpr (std::is_same_v<T, SetBlend>) {
				st.blend = cmd.mode;
			} else if constexpr (std::is_same_v<T, SetUniform1fSC>) {
				outU1f.push_back(cmd);
			} else if constexpr (std::is_same_v<T, SetUniform2fSC>) {
				outU2f.push_back(cmd);
			}
		},
		sc
	);
}

void RenderEncoder::Clear(PipeId pipe, Vec4 color) {
	Record(RecordedClear{ pipe, color });
}

// ----------------------------- Execute --------------------------------------
static int FindOrAddTex(std::vector<GLuint>& table, GLuint tex) {
	for (int i = 0; i < (int)table.size(); ++i) {
		if (table[i] == tex) {
			return i;
		}
	}
	if ((int)table.size() >= kMaxBatchTextures) {
		return -1;
	}
	table.push_back(tex);
	return (int)table.size() - 1;
}

void RenderEncoder::Execute(GLuint batchShader, GLuint whiteTex, int screenW, int screenH) {
	std::vector<ExecOp> ops;
	ops.reserve(recorded.size());

	StateSnapshot cur{};
	cur.vpW = screenW;
	cur.vpH = screenH;

	std::optional<DrawBatchOp> batch;
	std::vector<GLuint> batchTexTable;

	auto flushBatch = [&]() {
		if (batch && !batch->indices.empty()) {
			batch->textures = batchTexTable;
			ops.push_back(std::move(*batch));
		}
		batch.reset();
		batchTexTable.clear();
	};

	for (auto& cmd : recorded) {
		if (auto* clr = std::get_if<RecordedClear>(&cmd)) {
			flushBatch();

			ClearOp op;
			op.pipe	 = clr->pipe;
			op.color = clr->color;
			ops.push_back(std::move(op));

			continue;
		}

		auto& r				  = std::get<RecordedDraw>(cmd);
		StateSnapshot desired = cur;
		std::vector<SetUniform1fSC> u1f;
		std::vector<SetUniform2fSC> u2f;

		// Detect feedback loop: Out(pipe) + In(... same pipe ...)
		// => Ensure pong exists before compiling state
		PipeId outPipe = 0;
		std::vector<PipeId> inPipes;
		for (auto& sc : r.state) {
			if (auto* spo = std::get_if<SetPipeOut>(&sc)) {
				outPipe = spo->pipe;
			}
			if (auto* bpi = std::get_if<BindPipeIn>(&sc)) {
				inPipes.push_back(bpi->pipe);
			}
		}
		if (outPipe != 0) {
			for (auto p : inPipes) {
				if (p == outPipe) {
					EnsurePong(outPipe);
					break;
				}
			}
		}

		for (auto& sc : r.state) {
			ApplyStateChange(desired, sc, u1f, u2f);
		}

		if (r.kind == DrawKind::BatchGeom) {
			BatchKey key{ desired.framebuffer, desired.blend, desired.vpW, desired.vpH };

			if (!batch || !(batch->key == key)) {
				flushBatch();
				batch	   = DrawBatchOp{};
				batch->key = key;
				batchTexTable.clear();
				batchTexTable.push_back(whiteTex);
			}

			GLuint resolvedTex = ResolveTexture(r.texture);
			if (resolvedTex == 0) {
				resolvedTex = whiteTex;
			}

			int slot = FindOrAddTex(batchTexTable, resolvedTex);
			if (slot < 0) {
				flushBatch();
				batch	   = DrawBatchOp{};
				batch->key = key;
				batchTexTable.clear();
				batchTexTable.push_back(whiteTex);
				slot = FindOrAddTex(batchTexTable, resolvedTex);
				assert(slot >= 0);
			}

			uint32_t base = (uint32_t)batch->vertices.size();
			batch->vertices.insert(batch->vertices.end(), r.vertices.begin(), r.vertices.end());
			for (size_t vi = base; vi < batch->vertices.size(); ++vi) {
				batch->vertices[vi].texIndex = (float)slot;
			}
			for (auto idx : r.indices) {
				batch->indices.push_back(base + idx);
			}

			cur = desired;
		} else {
			flushBatch();
			DrawImmediateOp op;
			op.state	= desired;
			op.vertices = r.vertices;
			op.indices	= r.indices;
			op.u1f		= std::move(u1f);
			op.u2f		= std::move(u2f);

			for (auto it = r.state.rbegin(); it != r.state.rend(); ++it) {
				if (auto* spo = std::get_if<SetPipeOut>(&*it)) {
					op.swapPipeAfter = spo->pipe;
					break;
				}
			}
			ops.push_back(std::move(op));
			cur = desired;
		}
	}
	flushBatch();
	recorded.clear();

	GLMeshStream stream;
	stream.Init();
	BindVertexArray(stream.vao);

	auto applyState = [&](const StateSnapshot& st) {
		static GLuint curFbo	  = ~0u;
		static GLuint curShader	  = ~0u;
		static BlendMode curBlend = (BlendMode)999;
		static int curW = -1, curH = -1;
		static GLuint curTex[StateSnapshot::kMaxSlots]{};

		if (curFbo != st.framebuffer) {
			BindFramebuffer(GL_FRAMEBUFFER, st.framebuffer);
			curFbo = st.framebuffer;
		}
		if (curW != st.vpW || curH != st.vpH) {
			glViewport(0, 0, st.vpW, st.vpH);
			curW = st.vpW;
			curH = st.vpH;
		}
		if (curBlend != st.blend) {
			ApplyBlendMode(st.blend);
			curBlend = st.blend;
		}
		if (curShader != st.shader) {
			UseProgram(st.shader);
			curShader = st.shader;
		}
		for (int s = 0; s < StateSnapshot::kMaxSlots; s++) {
			if (curTex[s] != st.texSlots[s]) {
				ActiveTexture(GL_TEXTURE0 + s);
				glBindTexture(GL_TEXTURE_2D, st.texSlots[s]);
				curTex[s] = st.texSlots[s];
			}
		}
	};

	for (auto& op : ops) {
		if (auto* c = std::get_if<ClearOp>(&op)) {
			Pipe& p = pipes.at(c->pipe);

			// Clearing is a write: must clear current WriteFbo
			BindFramebuffer(GL_FRAMEBUFFER, p.WriteFbo());
			glViewport(0, 0, p.W(), p.H());
			glDisable(GL_BLEND);

			glClearColor(c->color.x, c->color.y, c->color.z, c->color.w);
			glClear(GL_COLOR_BUFFER_BIT);

			// important: advance ping/pong
			p.SwapAfterWrite();
			continue;
		}

		if (auto* b = std::get_if<DrawBatchOp>(&op)) {
			StateSnapshot st{};
			st.framebuffer = b->key.framebuffer;
			st.shader	   = batchShader;
			st.blend	   = b->key.blend;
			st.vpW		   = b->key.vpW;
			st.vpH		   = b->key.vpH;
			applyState(st);

			for (int s = 0; s < (int)b->textures.size(); ++s) {
				ActiveTexture(GL_TEXTURE0 + s);
				glBindTexture(GL_TEXTURE_2D, b->textures[s]);
			}
			for (int i = 0; i < kMaxBatchTextures; i++) {
				char name[32];
				std::snprintf(name, sizeof(name), "u_Textures[%d]", i);
				SetUniform1i(batchShader, name, i);
			}

			BindBuffer(GL_ARRAY_BUFFER, stream.vbo);
			BufferData(
				GL_ARRAY_BUFFER, b->vertices.size() * sizeof(Vertex), b->vertices.data(),
				GL_STREAM_DRAW
			);
			BindBuffer(GL_ELEMENT_ARRAY_BUFFER, stream.ebo);
			BufferData(
				GL_ELEMENT_ARRAY_BUFFER, b->indices.size() * sizeof(uint32_t), b->indices.data(),
				GL_STREAM_DRAW
			);
			glDrawElements(GL_TRIANGLES, (GLsizei)b->indices.size(), GL_UNSIGNED_INT, 0);
		} else {
			auto& im = std::get<DrawImmediateOp>(op);
			applyState(im.state);

			for (auto& u : im.u1f) {
				SetUniform1f(im.state.shader, u.name, u.v);
			}
			for (auto& u : im.u2f) {
				SetUniform2f(im.state.shader, u.name, u.v);
			}
			SetUniform1i(im.state.shader, "u_Texture", 0);

			BindBuffer(GL_ARRAY_BUFFER, stream.vbo);
			BufferData(
				GL_ARRAY_BUFFER, im.vertices.size() * sizeof(Vertex), im.vertices.data(),
				GL_STREAM_DRAW
			);
			BindBuffer(GL_ELEMENT_ARRAY_BUFFER, stream.ebo);
			BufferData(
				GL_ELEMENT_ARRAY_BUFFER, im.indices.size() * sizeof(uint32_t), im.indices.data(),
				GL_STREAM_DRAW
			);
			glDrawElements(GL_TRIANGLES, (GLsizei)im.indices.size(), GL_UNSIGNED_INT, 0);

			if (im.swapPipeAfter) {
				pipes.at(*im.swapPipeAfter).SwapAfterWrite();
			}
		}
	}

	stream.Destroy();
}

void RenderEncoder::Present(PipeId pipe, GLuint presentShader, int screenW, int screenH) {
	BindFramebuffer(GL_FRAMEBUFFER, 0);
	glViewport(0, 0, screenW, screenH);
	glDisable(GL_BLEND);
	UseProgram(presentShader);

	ActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, GetPipe(pipe).ReadTex());
	SetUniform1i(presentShader, "u_Texture", 0);

	auto v = FullscreenQuadV();
	auto i = FullscreenQuadI();

	GLMeshStream stream;
	stream.Init();
	BindVertexArray(stream.vao);
	BindBuffer(GL_ARRAY_BUFFER, stream.vbo);
	BufferData(GL_ARRAY_BUFFER, sizeof(v), v.data(), GL_STREAM_DRAW);
	BindBuffer(GL_ELEMENT_ARRAY_BUFFER, stream.ebo);
	BufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(i), i.data(), GL_STREAM_DRAW);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	stream.Destroy();
}

// -------------------------------- Shaders -----------------------------------
static const char* kVS_Batch = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(location=3) in float aTexIndex;
out vec2 vUV;
out vec4 vColor;
flat out int vTexIndex;
void main() {
    vUV=aUV; vColor=aColor; vTexIndex=int(aTexIndex+0.5);
    gl_Position=vec4(aPos,0,1);
}
)";

static const char* kFS_Batch = R"(
#version 330 core
in vec2 vUV;
in vec4 vColor;
flat in int vTexIndex;

out vec4 FragColor;

uniform sampler2D u_Textures[16];

void main()
{
    vec4 texColor = vColor;

    // Manual "switch" to avoid dynamic sampler array indexing on GL 3.3.
    // vTexIndex is expected to be 0..15 (0 = white).
    if      (vTexIndex == 0.0f)  texColor *= texture(u_Textures[0],  vUV);
    else if (vTexIndex == 1.0f)  texColor *= texture(u_Textures[1],  vUV);
    else if (vTexIndex == 2.0f)  texColor *= texture(u_Textures[2],  vUV);
    else if (vTexIndex == 3.0f)  texColor *= texture(u_Textures[3],  vUV);
    else if (vTexIndex == 4.0f)  texColor *= texture(u_Textures[4],  vUV);
    else if (vTexIndex == 5.0f)  texColor *= texture(u_Textures[5],  vUV);
    else if (vTexIndex == 6.0f)  texColor *= texture(u_Textures[6],  vUV);
    else if (vTexIndex == 7.0f)  texColor *= texture(u_Textures[7],  vUV);
    else if (vTexIndex == 8.0f)  texColor *= texture(u_Textures[8],  vUV);
    else if (vTexIndex == 9.0f)  texColor *= texture(u_Textures[9],  vUV);
    else if (vTexIndex == 10.0f) texColor *= texture(u_Textures[10], vUV);
    else if (vTexIndex == 11.0f) texColor *= texture(u_Textures[11], vUV);
    else if (vTexIndex == 12.0f) texColor *= texture(u_Textures[12], vUV);
    else if (vTexIndex == 13.0f) texColor *= texture(u_Textures[13], vUV);
    else if (vTexIndex == 14.0f) texColor *= texture(u_Textures[14], vUV);
    else if (vTexIndex == 15.0f) texColor *= texture(u_Textures[15], vUV);

    FragColor = texColor;
}

)";

static const char* kVS_Immediate = R"(
#version 330 core
layout(location=0) in vec2 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
layout(location=3) in float aTexIndex;
out vec2 vUV;
void main(){ vUV=aUV; gl_Position=vec4(aPos,0,1); }
)";

static const char* kFS_Present = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_Texture;
void main(){ FragColor=texture(u_Texture, vUV); }
)";

static const char* kFS_Threshold = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform float u_Threshold;
void main(){
    vec3 c=texture(u_Texture,vUV).rgb;
    float l=max(max(c.r,c.g),c.b);
    FragColor=(l>u_Threshold)?vec4(c,1):vec4(0,0,0,1);
}
)";

static const char* kFS_Grayscale = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_Texture;
void main(){
    vec3 c=texture(u_Texture,vUV).rgb;
    float g=dot(c,vec3(0.299,0.587,0.114));
    FragColor=vec4(g,g,g,1);
}
)";

static const char* kFS_Blur = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D u_Texture;
uniform vec2 u_TexelSize;
uniform vec2 u_Direction;
void main(){
    vec3 sum=vec3(0);
    float w0=0.227027;
    float w1=0.316216;
    float w2=0.070270;
    sum += texture(u_Texture, vUV).rgb*w0;
    sum += texture(u_Texture, vUV + u_Direction*u_TexelSize*1.384615).rgb*w1;
    sum += texture(u_Texture, vUV - u_Direction*u_TexelSize*1.384615).rgb*w1;
    sum += texture(u_Texture, vUV + u_Direction*u_TexelSize*3.230769).rgb*w2;
    sum += texture(u_Texture, vUV - u_Direction*u_TexelSize*3.230769).rgb*w2;
    FragColor=vec4(sum,1);
}
)";

static const char* kFS_Light = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
void main(){
    vec2 p=vUV*2.0-1.0;
    float d=length(p);
    float intensity=smoothstep(1.0,0.0,d);
    FragColor=vec4(vec3(intensity),1);
}
)";

static GLuint CreateWhiteTexture() {
	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	uint32_t white = 0xFFFFFFFFu;
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &white);
	return tex;
}

} // namespace ptgn

// ----------------------------------- main -----------------------------------
int main() {
	using namespace ptgn;

	if (!SDL_Init(SDL_INIT_VIDEO)) {
		std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
		return 1;
	}

	// OpenGL 3.3 core
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);

	int W = 1280, H = 720;

	SDL_Window* win = SDL_CreateWindow(
		"HDR Pool + Resize Pipes (SDL3)", W, H, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
	);

	if (!win) {
		std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
		return 1;
	}

	SDL_GLContext ctx = SDL_GL_CreateContext(win);
	if (!ctx) {
		std::cerr << "SDL_GL_CreateContext failed: " << SDL_GetError() << "\n";
		return 1;
	}

	SDL_GL_MakeCurrent(win, ctx);
	SDL_GL_SetSwapInterval(1);

	ptgn::impl::gl::LoadGLFunctions();

	GLuint whiteTex = CreateWhiteTexture();

	GLuint batchShader	 = ptgn::LinkProgram(kVS_Batch, kFS_Batch);
	GLuint presentShader = ptgn::LinkProgram(kVS_Immediate, kFS_Present);

	GLuint thresholdShader = ptgn::LinkProgram(kVS_Immediate, kFS_Threshold);
	GLuint grayscaleShader = ptgn::LinkProgram(kVS_Immediate, kFS_Grayscale);
	GLuint blurHShader	   = ptgn::LinkProgram(kVS_Immediate, kFS_Blur);
	GLuint blurVShader	   = ptgn::LinkProgram(kVS_Immediate, kFS_Blur);
	GLuint lightShader	   = ptgn::LinkProgram(kVS_Immediate, kFS_Light);

	RenderTargetPool pool(240);
	RenderEncoder enc(pool);

	GLenum sceneFmt = GL_RGBA16F;
	PipeId scene	= enc.CreatePipe(W, H, sceneFmt); // ping only

	uint64_t frame = 0;
	bool running   = true;

	while (running) {
		SDL_Event ev;
		while (SDL_PollEvent(&ev)) {
			if (ev.type == SDL_EVENT_QUIT) {
				running = false;
			}
		}

		enc.BeginFrame(frame);

		int newW = 0, newH = 0;
		SDL_GetWindowSizeInPixels(win, &newW, &newH);
		if (newW <= 0 || newH <= 0) {
			continue;
		}

		if (newW != W || newH != H) {
			W = newW;
			H = newH;
			enc.ResizePipe(scene, W, H);
		}

		enc.Clear(scene, { 0, 0, 0, 1 });

		// Record scene content
		enc.DrawRect(scene, { 0, 0 }, { 0.4f, 0.25f }, { 1, 0, 0, 0.85f });
		// enc.DrawCircle(scene, { 0.5f, 0.2f }, 0.15f, { 0, 1, 0, 0.85f });
		/*enc.DrawSprite(
			scene, { -0.6f, -0.2f }, { 0.25f, 0.25f }, White(), TextureRef::FromTex(whiteTex),
			BlendMode::Alpha
		);*/

		// Bloom: fork uses ping only (aliased read)

		/*PipeId bloom = enc.Fork(scene);

		auto fsV = FullscreenQuadV();
		auto fsI = FullscreenQuadI();

		enc.Pass()
			.Out(bloom)
			.In(0, scene)
			.Shader(thresholdShader)
			.Uniform1f("u_Threshold", 0.65f)
			.Draw(fsV, fsI);*/
		/*

		// blur bloom reads+writes bloom => pong will be lazily allocated
		int iters = 3;
		Vec2 texel{ 1.0f / (float)enc.GetPipe(bloom).W(), 1.0f / (float)enc.GetPipe(bloom).H() };
		for (int i = 0; i < iters; i++) {
			enc.Pass()
				.Out(bloom)
				.In(0, bloom)
				.Shader(blurHShader)
				.Uniform2f("u_TexelSize", texel)
				.Uniform2f("u_Direction", { 1, 0 })
				.Draw(fsV, fsI);
			enc.Pass()
				.Out(bloom)
				.In(0, bloom)
				.Shader(blurVShader)
				.Uniform2f("u_TexelSize", texel)
				.Uniform2f("u_Direction", { 0, 1 })
				.Draw(fsV, fsI);
		}*/

		/*enc.DrawSprite(
			scene, { 0, 0 }, { 1, 1 }, White(), TextureRef::FromPipe(bloom), BlendMode::AdditiveRGBA
		);*/

		// Lighting pipe: ping only; blur will allocate pong if needed
		// PipeId lightPipe = enc.CreatePipe(W, H, sceneFmt);
		// enc.DrawLight(scene, lightPipe, lightShader, 1, blurHShader, blurVShader);

		// Post grayscale reads+writes scene => pong will be lazily allocated
		enc.DrawGrayscale(scene, grayscaleShader);

		enc.Execute(batchShader, whiteTex, W, H);
		enc.Present(scene, presentShader, W, H);

		// enc.ReleasePipe(bloom);
		//   enc.ReleasePipe(lightPipe);

		pool.GarbageCollect(frame);

		SDL_GL_SwapWindow(win);
		frame++;
	}

	SDL_GL_DestroyContext(ctx);
	SDL_DestroyWindow(win);
	SDL_Quit();
	return 0;
}