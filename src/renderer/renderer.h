#pragma once

#include <array>

#include "protegon/color.h"
#include "protegon/polygon.h"
#include "protegon/shader.h"
#include "protegon/texture.h"
#include "protegon/vertex_array.h"

namespace ptgn {

namespace impl {

class GameInstance;

} // namespace impl

// class SpriteBatch {
// public:
//	// when we know the specific texture and source rectangle
//	void Draw(TextureHandle tex, Rect source, /*...*/);
//
//	// helper for drawing a whole texture
//	void Draw(TextureHandle tex, /*...*/) {
//		Draw(tex, Texture_SizeOf(text), /*...*/);
//	}
//
//	// helper if we have an atlas and a specific index of a sub-texture therein
//	void Draw(TextureAtlasHandle atlas, int index, /*...*/) {
//		Draw(TextureAtlas_TextureOf(atlas), TextureAtlas_RectOf(atlas, index), /*...*/);
//	}
//
//	// helper if we have an abstract sprite handle without details about a specific atlas
//	void Draw(SpriteFrameHandle sprite, /*...*/) {
//		Draw(SpriteFrame_SheetOf(sprite), SpriteFrame_IndexOf(sprite), /*...*/);
//	}
// };

// if (blendMode != data->current.blendMode) {
//	switch (blendMode) {
//		case SDL_BLENDMODE_NONE:
//			data->glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
//			data->glDisable(GL_BLEND);
//			break;
//		case SDL_BLENDMODE_BLEND:
//			data->glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//			data->glEnable(GL_BLEND);
//			data->glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
//			break;
//		case SDL_BLENDMODE_ADD:
//			data->glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//			data->glEnable(GL_BLEND);
//			data->glBlendFunc(GL_SRC_ALPHA, GL_ONE);
//			break;
//		case SDL_BLENDMODE_MOD:
//			data->glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
//			data->glEnable(GL_BLEND);
//			data->glBlendFunc(GL_ZERO, GL_SRC_COLOR);
//			break;
//	}
//	data->current.blendMode = blendMode;
// }

enum class BlendMode {
	// Source: https://wiki.libsdl.org/SDL2/SDL_BlendMode

	None  = 0x00000000,	   /*       no blending: dstRGBA = srcRGBA */
	Blend = 0x00000001,	   /*    alpha blending: dstRGB = (srcRGB * srcA) + (dstRGB
							* (1 - srcA)) dstA = srcA + (dstA * (1-srcA)) */
	Add = 0x00000002,	   /* additive blending: dstRGB = (srcRGB * srcA) + dstRGB
													  dstA = dstA */
	Modulate = 0x00000004, /*    color modulate: dstRGB = srcRGB * dstRGB
												 dstA = dstA */
	Multiply = 0x00000008, /*    color multiply: dstRGB = (srcRGB * dstRGB) +
							  (dstRGB * (1 - srcA)) dstA = dstA */
	Invalid = 0x7FFFFFFF
};

enum class Flip {
	// Source: https://wiki.libsdl.org/SDL2/SDL_RendererFlip

	None	   = 0x00000000,
	Horizontal = 0x00000001,
	Vertical   = 0x00000002
};

struct QuadVertex {
	glsl::vec3 position;
	glsl::vec4 color;
	glsl::vec2 tex_coord;
	glsl::float_ tex_index;
	glsl::float_ tiling_factor;
};

// struct CircleVertex {
//	glsl::vec3 world_position;
//	glsl::vec3 local_position;
//	glsl::vec4 color;
//	glsl::float_ thickness;
//	glsl::float_ fade;
// };
//
// struct LineVertex {
//	glsl::vec3 position;
//	glsl::vec4 color;
// };
//
// struct TextVertex {
//	glsl::vec3 position;
//	glsl::vec4 color;
//	glsl::vec2 tex_coord;
// };

class Renderer {
private:
	Renderer()							 = default;
	~Renderer()							 = default;
	Renderer(const Renderer&)			 = delete;
	Renderer(Renderer&&)				 = default;
	Renderer& operator=(const Renderer&) = delete;
	Renderer& operator=(Renderer&&)		 = default;

public:
	void SetClearColor(const Color& color) const;
	void Clear() const;
	void Present();
	void SetViewport(const V2_int& size);

	void Submit(const VertexArray& va, const Shader& shader);

	// Primitives
	void DrawQuad(const V2_float& position, const V2_float& size, const V4_float& color);
	void DrawQuad(const V3_float& position, const V2_float& size, const V4_float& color);
	void DrawQuad(
		const V2_float& position, const V2_float& size, const Texture& texture,
		float tilingFactor = 1.0f, const V4_float& tintColor = V4_float(1.0f)
	);
	void DrawQuad(
		const V3_float& position, const V2_float& size, const Texture& texture,
		float tilingFactor = 1.0f, const V4_float& tintColor = V4_float(1.0f)
	);

	void DrawQuad(const M4_float& transform, const V4_float& color);
	void DrawQuad(
		const M4_float& transform, const Texture& texture, float tilingFactor = 1.0f,
		const V4_float& tintColor = V4_float(1.0f)
	);

	void DrawRotatedQuad(
		const V2_float& position, const V2_float& size, float rotation, const V4_float& color
	);

	void DrawRotatedQuad(
		const V3_float& position, const V2_float& size, float rotation, const V4_float& color
	);
	void DrawRotatedQuad(
		const V2_float& position, const V2_float& size, float rotation, const Texture& texture,
		float tilingFactor = 1.0f, const V4_float& tintColor = V4_float(1.0f)
	);
	void DrawRotatedQuad(
		const V3_float& position, const V2_float& size, float rotation, const Texture& texture,
		float tilingFactor = 1.0f, const V4_float& tintColor = V4_float(1.0f)
	);

	/*void DrawCircle(
		const M4_float& transform, const V4_float& color, float thickness = 1.0f,
		float fade = 0.005f, int entityID = -1
	);

	void DrawLine(
		const V3_float& p0, V3_float& p1, const V4_float& color, int entityID = -1
	);*/

	/*void DrawRect(const V3_float& position, const V2_float& size, const V4_float& color);
	void DrawRect(const M4_float& transform, const V4_float& color);*/

	// void DrawSprite(const M4_float& transform, SpriteRendererComponent& src);

	/*struct TextParams {
		V4_float Color{ 1.0f };
		float Kerning	  = 0.0f;
		float LineSpacing = 0.0f;
	};*/

	/*void DrawString(
		const std::string& string, std::shared_ptr<Font> font, const M4_float& transform,
		const TextParams& textParams, int entityID = -1
	);
	void DrawString(
		const std::string& string, const M4_float& transform, const TextComponent& component,
		int entityID = -1
	);*/

	/*float GetLineWidth();
	void SetLineWidth(float width);*/

	// Stats
	/*struct Statistics {
		uint32_t DrawCalls = 0;
		uint32_t QuadCount = 0;

		uint32_t GetTotalVertexCount() const {
			return QuadCount * 4;
		}

		uint32_t GetTotalIndexCount() const {
			return QuadCount * 6;
		}
	};*/

	/*void ResetStats();
	Statistics GetStats();*/

private:
	friend class Game;
	friend class impl::GameInstance;
	void Init();
	void StartBatch();
	void NextBatch();

	// static void BeginScene(const Camera& camera, const M4_float& transform);
	// static void BeginScene(const EditorCamera& camera);
	// static void BeginScene(const OrthographicCamera& camera); // TODO: Remove
	// static void EndScene();
	void Flush();

	struct RendererData {
		template <typename TVertex>
		struct BatchData {
			VertexArray array_;
			VertexBuffer buffer_;
			Shader shader_;
			std::uint32_t index_count_ = 0;
			std::vector<TVertex> buffer_base_;
			TVertex* buffer_ptr_ = nullptr;
		};

		constexpr static const std::uint32_t max_quads_	   = 20000;
		constexpr static const std::uint32_t max_vertices_ = max_quads_ * 4;
		constexpr static const std::uint32_t max_indices_  = max_quads_ * 6;
		std::uint32_t max_texture_slots_{ 0 };

		V4_float quad_vertex_positions_[4];
		M4_float view_projection_;

		Texture white_texture_;

		std::vector<Texture> texture_slots_;   // max_texture_slots
		std::uint32_t texture_slot_index_ = 1; // 0 = white texture

		BatchData<QuadVertex> quad_;
		/*BatchData<CircleVertex> circle_;
		BatchData<LineVertex> line_;
		BatchData<TextVertex> text_;

		float line_width_ = 2.0f;*/

		// Texture font_atlas_texture_;
	};

	RendererData data_;
};

} // namespace ptgn
