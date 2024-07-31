#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"

namespace ptgn {

namespace impl {

template <>
inline void BatchData<QuadVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		data.BindTextures();
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

template <>
inline void BatchData<CircleVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

template <>
inline void BatchData<LineVertex>::Draw(RendererData& data) {
	if (index_count_) {
		SetupBatch();
		GLRenderer::SetLineWidth(data.line_width_);
		GLRenderer::DrawElements(array_, index_count_);
		data.stats_.draw_calls++;
	}
}

// template <>
// inline void BatchData<TextVertex>::Draw(RendererData& data) {
//	if (index_count_) {
//		SetupBatch();
//		data.font_atlas_texture_.Bind(0);
//		GLRenderer::DrawIndexed(array_, index_count_);
//		// stats.draw_calls_++;
//	}
// }

template <>
void BatchData<QuadVertex>::AddQuad(
	const M4_float& transform, const V4_float& color, const std::array<V2_float, 4>& tex_coords,
	float texture_index, float tiling_factor
) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	constexpr auto rel_vertices{ GetRelativeVertices() };
	for (size_t i = 0; i < QuadVertex::VertexCount(); i++) {
		auto pos				   = transform * rel_vertices[i];
		buffer_ptr_->position	   = { pos.x, pos.y, pos.z };
		buffer_ptr_->color		   = { color.x, color.y, color.z, color.w };
		buffer_ptr_->tex_coord	   = { tex_coords[i].x, tex_coords[i].y };
		buffer_ptr_->tex_index	   = { texture_index };
		buffer_ptr_->tiling_factor = { tiling_factor };
		IncrementBufferPtr();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<CircleVertex>::AddCircle(
	const M4_float& transform, const V4_float& color, float thickness, float fade
) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	constexpr auto rel_vertices{ GetRelativeVertices() };
	for (std::size_t i{ 0 }; i < QuadVertex::VertexCount(); i++) {
		auto pos					= transform * rel_vertices[i];
		auto local_pos				= rel_vertices[i] * 2.0f;
		buffer_ptr_->world_position = { pos.x, pos.y, pos.z };
		buffer_ptr_->local_position = { local_pos.x, local_pos.y, local_pos.z };
		buffer_ptr_->color			= { color.x, color.y, color.z, color.w };
		buffer_ptr_->thickness		= { thickness };
		buffer_ptr_->fade			= { fade };
		IncrementBufferPtr();
	}

	index_count_ += QuadVertex::IndexCount();
}

template <>
void BatchData<LineVertex>::AddLine(const V3_float& p0, const V3_float& p1, const V4_float& color) {
	PTGN_ASSERT(buffer_ptr_ != nullptr);
	buffer_ptr_->position = { p0.x, p0.y, p0.z };
	buffer_ptr_->color	  = { color.x, color.y, color.z, color.w };
	IncrementBufferPtr();
	buffer_ptr_->position = { p1.x, p1.y, p1.z };
	buffer_ptr_->color	  = { color.x, color.y, color.z, color.w };
	IncrementBufferPtr();

	index_count_ += LineVertex::IndexCount();
}

void RendererData::Init() {
	SetupBuffers();
	SetupTextureSlots();
	SetupShaders();

	// TODO: Decide if necessary.
	// CameraUniformBuffer = UniformBuffer::Create(sizeof(CameraData), 0);
}

void RendererData::SetupBuffers() {
	IndexBuffer quad_index_buffer{ GetQuadIndices<max_indices_>() };

	quad_.Init<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>(
		max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	circle_.Init<glsl::vec3, glsl::vec3, glsl::vec4, glsl::float_, glsl::float_>(
		max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);

	/*
	// TODO: Fix
	line_.Init<glsl::vec3, glsl::vec4>(max_vertices_, PrimitiveMode::Lines, {});*/

	/*text_.Init<glsl::vec3, glsl::vec4, glsl::vec2>(
		max_vertices_, PrimitiveMode::Triangles, quad_index_buffer
	);*/
}

void RendererData::SetupTextureSlots() {
	// First texture slot is occupied by white texture
	std::uint32_t white_texture_data{ 0xffffffff };
	white_texture_ = Texture(&white_texture_data, { 1, 1 }, ImageFormat::RGBA8888);

	max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	texture_slots_.resize(max_texture_slots_);
	texture_slots_[0] = white_texture_;
}

void RendererData::SetupShaders() {
	PTGN_ASSERT(max_texture_slots_ > 0, "Max texture slots must be set before setting up shaders");

	std::vector<std::int32_t> samplers(max_texture_slots_);

	for (std::uint32_t i{ 0 }; i < samplers.size(); ++i) {
		samplers[i] = i;
	}

	quad_.SetupShader(
		"resources/shader/renderer_quad_vertex.glsl",
		"resources/shader/renderer_quad_fragment.glsl", samplers
	);
	circle_.SetupShader(
		"resources/shader/renderer_circle_vertex.glsl",
		"resources/shader/renderer_circle_fragment.glsl", samplers
	);
	line_.SetupShader(
		"resources/shader/renderer_line_vertex.glsl",
		"resources/shader/renderer_line_fragment.glsl", samplers
	);
	/*text_.SetupShader(
		"resources/shader/renderer_text_vertex.glsl",
		"resources/shader/renderer_text_fragment.glsl", samplers
	);*/
}

void RendererData::BindTextures() const {
	for (std::uint32_t i{ 0 }; i < texture_slot_index_; i++) {
		texture_slots_[i].Bind(i);
	}
}

void RendererData::Flush() {
	quad_.Draw(*this);
	circle_.Draw(*this);
	line_.Draw(*this);
	// text_.Draw();
}

} // namespace impl

void Renderer::SetClearColor(const Color& color) const {
	GLRenderer::SetClearColor(color);
}

void Renderer::Clear() const {
	GLRenderer::Clear();
}

void Renderer::Present(bool print_stats) {
	NextBatch();
	if (print_stats) {
		data_.stats_.Print();
	}
	data_.stats_.Reset();
	game.window.SwapBuffers();
}

void Renderer::Submit(const VertexArray& va, const Shader& shader) {
	shader.Bind();
	/*shader.SetUniform("u_ViewProjection", view_projection);
	 shader.SetUniform("u_Transform", transform);*/

	GLRenderer::DrawElements(va);
	data_.stats_.draw_calls++;
}

void Renderer::SetViewport(const V2_int& size) {
	GLRenderer::SetViewport({}, size);
}

void Renderer::Init() {
	GLRenderer::Init();
	SetViewport(game.window.GetSize());

	// Only update viewport after resizing finishes, not during (saves a few GPU calls).
	// If desired, changing the word Resized . Resizing will make the viewport update during
	// resizing.
	game.event.window.Subscribe(
		WindowEvent::Resized, (void*)this,
		std::function([&](const WindowResizedEvent& e) { SetViewport(e.size); })
	);

	data_.Init();

	StartBatch();
}

// void Renderer::BeginScene(const OrthographicCamera& camera) {
//	data_.CameraBuffer.ViewProjection = camera.GetViewProjectionMatrix();
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::BeginScene(const Camera& camera, const M4_float& transform) {
//	data_.CameraBuffer.ViewProjection = camera.GetProjection() * glm::inverse(transform);
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::BeginScene(const EditorCamera& camera) {
//	data_.CameraBuffer.ViewProjection = camera.GetViewProjection();
//	data_.CameraUniformBuffer.SetData(&data_.CameraBuffer, sizeof(data_.CameraData));
//
//	StartBatch();
// }
//
// void Renderer::EndScene() {
//	Flush();
// }
//
void Renderer::StartBatch() {
	data_.quad_.Reset();
	data_.circle_.Reset();
	data_.line_.Reset();
	// data_.text_.Reset();
	data_.texture_slot_index_ = 1;
}

void Renderer::Flush() {
	data_.Flush();
}

void Renderer::NextBatch() {
	Flush();
	StartBatch();
}

void Renderer::DrawQuad(const V2_float& position, const V2_float& size, const V4_float& color) {
	DrawQuad({ position.x, position.y, 0.0f }, size, color);
}

void Renderer::DrawQuad(const V3_float& position, const V2_float& size, const V4_float& color) {
	M4_float transform = M4_float::Translate(M4_float(1.0f), position) *
						 M4_float::Scale(M4_float(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, color);
}

void Renderer::DrawQuad(
	const V2_float& position, const V2_float& size, const Texture& texture, float tiling_factor,
	const V4_float& tint_color
) {
	DrawQuad({ position.x, position.y, 0.0f }, size, texture, tiling_factor, tint_color);
}

void Renderer::DrawQuad(
	const V3_float& position, const V2_float& size, const Texture& texture, float tiling_factor,
	const V4_float& tint_color
) {
	M4_float transform = M4_float::Translate(M4_float(1.0f), position) *
						 M4_float::Scale(M4_float(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, texture, tiling_factor, tint_color);
}

void Renderer::DrawQuad(const M4_float& transform, const V4_float& color) {
	constexpr auto texture_coords{ impl::RendererData::GetTextureCoordinates() };
	const float texture_index = 0.0f; // white texture
	const float tiling_factor = 1.0f;

	if (data_.quad_.index_count_ >= data_.max_indices_) {
		NextBatch();
	}

	data_.quad_.AddQuad(transform, color, texture_coords, texture_index, tiling_factor);
	data_.stats_.quad_count++;
}

void Renderer::DrawQuad(
	const M4_float& transform, const Texture& texture, float tiling_factor,
	const V4_float& tint_color
) {
	constexpr auto texture_coords{ impl::RendererData::GetTextureCoordinates() };

	if (data_.quad_.index_count_ >= data_.max_indices_) {
		NextBatch();
	}

	float texture_index{ 0.0f };

	for (std::uint32_t i{ 1 }; i < data_.texture_slot_index_; i++) {
		if (data_.texture_slots_[i].GetInstance() == texture.GetInstance()) {
			texture_index = (float)i;
			break;
		}
	}

	if (texture_index == 0.0f) {
		// TODO: Optimize this if you have time. Instead of flushing the batch when the slot index
		// is beyond the slots, keep a separate texture buffer and just split that one into two or
		// more batches. This should reduce draw calls drastically.
		if (data_.texture_slot_index_ >= data_.max_texture_slots_) {
			NextBatch();
		}

		texture_index									= (float)data_.texture_slot_index_;
		data_.texture_slots_[data_.texture_slot_index_] = texture;
		data_.texture_slot_index_++;
	}

	data_.quad_.AddQuad(transform, tint_color, texture_coords, texture_index, tiling_factor);
	data_.stats_.quad_count++;
}

void Renderer::DrawRotatedQuad(
	const V2_float& position, const V2_float& size, float rotation, const V4_float& color
) {
	DrawRotatedQuad({ position.x, position.y, 0.0f }, size, rotation, color);
}

void Renderer::DrawRotatedQuad(
	const V3_float& position, const V2_float& size, float rotation, const V4_float& color
) {
	M4_float transform =
		M4_float::Translate(M4_float(1.0f), position) *
		M4_float::Rotate(M4_float(1.0f), DegToRad(rotation), { 0.0f, 0.0f, 1.0f }) *
		M4_float::Scale(M4_float(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, color);
}

void Renderer::DrawRotatedQuad(
	const V2_float& position, const V2_float& size, float rotation, const Texture& texture,
	float tilingFactor, const V4_float& tintColor
) {
	DrawRotatedQuad(
		{ position.x, position.y, 0.0f }, size, rotation, texture, tilingFactor, tintColor
	);
}

void Renderer::DrawRotatedQuad(
	const V3_float& position, const V2_float& size, float rotation, const Texture& texture,
	float tilingFactor, const V4_float& tintColor
) {
	M4_float transform =
		M4_float::Translate(M4_float(1.0f), position) *
		M4_float::Rotate(M4_float(1.0f), DegToRad(rotation), { 0.0f, 0.0f, 1.0f }) *
		M4_float::Scale(M4_float(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, texture, tilingFactor, tintColor);
}

void Renderer::DrawCircle(
	const V2_float& position, float radius, const V4_float& color, float thickness /*= 1.0f*/,
	float fade /*= 0.005f*/
) {
	DrawCircle({ position.x, position.y, 0.0f }, radius, color, thickness, fade);
}

void Renderer::DrawCircle(
	const V3_float& position, float radius, const V4_float& color, float thickness /*= 1.0f*/,
	float fade /*= 0.005f*/
) {
	M4_float transform = M4_float::Translate(M4_float(1.0f), position) *
						 M4_float::Scale(M4_float(1.0f), { radius, radius, 1.0f });

	DrawCircle(transform, color, thickness, fade);
}

void Renderer::DrawCircle(
	const M4_float& transform, const V4_float& color, float thickness /*= 1.0f*/,
	float fade /*= 0.005f*/
) {
	if (data_.circle_.index_count_ >= data_.max_indices_) {
		NextBatch();
	}

	data_.circle_.AddCircle(transform, color, thickness, fade);
	data_.stats_.circle_count++;
}

void Renderer::DrawLine(const V3_float& p0, V3_float& p1, const V4_float& color) {
	data_.line_.AddLine(p0, p1, color);
	data_.stats_.line_count++;
}

void Renderer::DrawRect(const V3_float& position, const V2_float& size, const V4_float& color) {
	float half_width{ size.x * 0.5f };
	float half_height{ size.y * 0.5f };

	V3_float p0{ position.x - half_width, position.y - half_height, position.z };
	V3_float p1{ position.x + half_width, position.y - half_height, position.z };
	V3_float p2{ position.x + half_width, position.y + half_height, position.z };
	V3_float p3{ position.x - half_width, position.y + half_height, position.z };

	DrawLine(p0, p1, color);
	DrawLine(p1, p2, color);
	DrawLine(p2, p3, color);
	DrawLine(p3, p0, color);
}

void Renderer::DrawRect(const M4_float& transform, const V4_float& color) {
	auto vertices = impl::BatchData<QuadVertex>::GetQuadVertexPositions(transform);
	PTGN_ASSERT(vertices.size() == QuadVertex::VertexCount());

	DrawLine(vertices[0], vertices[1], color);
	DrawLine(vertices[1], vertices[2], color);
	DrawLine(vertices[2], vertices[3], color);
	DrawLine(vertices[3], vertices[0], color);
}

// void Renderer::DrawSprite(const M4_float& transform, SpriteRendererComponent& src) {
//	if (src.Texture) {
//		DrawQuad(transform, src.Texture, src.TilingFactor, src.Color);
//	} else {
//		DrawQuad(transform, src.Color);
//	}
// }

// void Renderer::DrawString(
//	const std::string& string, std::shared_ptr<Font> font, const M4_float& transform,
//	const TextParams& textParams, int entityID
//) {
//	const auto& fontGeometry = font.GetMSDFData().FontGeometry;
//	const auto& metrics		 = fontGeometry.getMetrics();
//	Texture fontAtlas		 = font.GetAtlasTexture();
//
//	data_.FontAtlasTexture = fontAtlas;
//
//	double x	   = 0.0;
//	double fsScale = 1.0 / (metrics.ascenderY - metrics.descenderY);
//	double y	   = 0.0;
//
//	const float spaceGlyphAdvance = fontGeometry.getGlyph(' ').getAdvance();
//
//	for (size_t i = 0; i < string.size(); i++) {
//		char character = string[i];
//		if (character == '\r') {
//			continue;
//		}
//
//		if (character == '\n') {
//			x  = 0;
//			y -= fsScale * metrics.lineHeight + textParams.LineSpacing;
//			continue;
//		}
//
//		if (character == ' ') {
//			float advance = spaceGlyphAdvance;
//			if (i < string.size() - 1) {
//				char nextCharacter = string[i + 1];
//				double dAdvance;
//				fontGeometry.getAdvance(dAdvance, character, nextCharacter);
//				advance = (float)dAdvance;
//			}
//
//			x += fsScale * advance + textParams.Kerning;
//			continue;
//		}
//
//		if (character == '\t') {
//			// NOTE(Yan): is this right?
//			x += 4.0f * (fsScale * spaceGlyphAdvance + textParams.Kerning);
//			continue;
//		}
//
//		auto glyph = fontGeometry.getGlyph(character);
//		if (!glyph) {
//			glyph = fontGeometry.getGlyph('?');
//		}
//		if (!glyph) {
//			return;
//		}
//
//		double al, ab, ar, at;
//		glyph.getQuadAtlasBounds(al, ab, ar, at);
//		V2_float texCoordMin((float)al, (float)ab);
//		V2_float texCoordMax((float)ar, (float)at);
//
//		double pl, pb, pr, pt;
//		glyph.getQuadPlaneBounds(pl, pb, pr, pt);
//		V2_float quadMin((float)pl, (float)pb);
//		V2_float quadMax((float)pr, (float)pt);
//
//		quadMin *= fsScale, quadMax *= fsScale;
//		quadMin += V2_float(x, y);
//		quadMax += V2_float(x, y);
//
//		float texelWidth   = 1.0f / fontAtlas.GetWidth();
//		float texelHeight  = 1.0f / fontAtlas.GetHeight();
//		texCoordMin		  *= V2_float(texelWidth, texelHeight);
//		texCoordMax		  *= V2_float(texelWidth, texelHeight);
//
//		// render here
//		data_.text_.buffer_ptr_.Position = transform * V4_float(quadMin, 0.0f, 1.0f);
//		data_.text_.buffer_ptr_.Color	 = textParams.Color;
//		data_.text_.buffer_ptr_.TexCoord = texCoordMin;
//		data_.text_.buffer_ptr_.EntityID = entityID;
//		data_.text_.buffer_ptr_++;
//
//		data_.text_.buffer_ptr_.Position = transform * V4_float(quadMin.x, quadMax.y, 0.0f, 1.0f);
//		data_.text_.buffer_ptr_.Color	 = textParams.Color;
//		data_.text_.buffer_ptr_.TexCoord = { texCoordMin.x, texCoordMax.y };
//		data_.text_.buffer_ptr_.EntityID = entityID;
//		data_.text_.buffer_ptr_++;
//
//		data_.text_.buffer_ptr_.Position = transform * V4_float(quadMax, 0.0f, 1.0f);
//		data_.text_.buffer_ptr_.Color	 = textParams.Color;
//		data_.text_.buffer_ptr_.TexCoord = texCoordMax;
//		data_.text_.buffer_ptr_.EntityID = entityID;
//		data_.text_.buffer_ptr_++;
//
//		data_.text_.buffer_ptr_.Position = transform * V4_float(quadMax.x, quadMin.y, 0.0f, 1.0f);
//		data_.text_.buffer_ptr_.Color	 = textParams.Color;
//		data_.text_.buffer_ptr_.TexCoord = { texCoordMax.x, texCoordMin.y };
//		data_.text_.buffer_ptr_.EntityID = entityID;
//		data_.text_.buffer_ptr_++;
//
//		data_.text_.index_count_ += 6;
//		data_.Stats.QuadCount++;
//
//		if (i < string.size() - 1) {
//			double advance	   = glyph.getAdvance();
//			char nextCharacter = string[i + 1];
//			fontGeometry.getAdvance(advance, character, nextCharacter);
//
//			x += fsScale * advance + textParams.Kerning;
//		}
//	}
// }
//
// void Renderer::DrawString(
//	const std::string& string, const M4_float& transform, const TextComponent& component,
//	int entityID
//) {
//	DrawString(
//		string, component.FontAsset, transform,
//		{ component.Color, component.Kerning, component.LineSpacing }, entityID
//	);
// }
//
// float Renderer::GetLineWidth() {
//	return data_.LineWidth;
// }
//
// void Renderer::SetLineWidth(float width) {
//	data_.LineWidth = width;
// }
//

} // namespace ptgn