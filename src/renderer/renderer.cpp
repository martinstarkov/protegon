#include "renderer.h"

#include "protegon/game.h"
#include "protegon/texture.h"
#include "renderer/gl_renderer.h"

namespace ptgn {

void Renderer::SetClearColor(const Color& color) const {
	GLRenderer::SetClearColor(color);
}

void Renderer::Clear() const {
	GLRenderer::Clear();
}

void Renderer::Present() {
	// TODO: Fix
	// NextBatch();
	game.window.SwapBuffers();
}

void Renderer::Submit(const VertexArray& va, const Shader& shader) {
	shader.Bind();
	/*shader.SetUniform("u_ViewProjection", view_projection);
	 shader.SetUniform("u_Transform", transform);*/

	GLRenderer::DrawElements(va);
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

	data_.quad_.array_.SetPrimitiveMode(PrimitiveMode::Triangles);

	data_.quad_.buffer_base_.resize(data_.max_vertices_);
	data_.quad_.buffer_ = VertexBuffer(data_.quad_.buffer_base_);
	data_.quad_.buffer_.SetLayout<glsl::vec3, glsl::vec4, glsl::vec2, glsl::float_, glsl::float_>();

	data_.quad_.array_.SetVertexBuffer(data_.quad_.buffer_);

	std::vector<std::uint32_t> quad_indices;
	quad_indices.resize(data_.max_indices_, 0);

	std::uint32_t offset = 0;
	for (std::uint32_t i = 0; i < data_.max_indices_; i += 6) {
		quad_indices[i + 0] = offset + 0;
		quad_indices[i + 1] = offset + 1;
		quad_indices[i + 2] = offset + 2;

		quad_indices[i + 3] = offset + 2;
		quad_indices[i + 4] = offset + 3;
		quad_indices[i + 5] = offset + 0;

		offset += 4;
	}

	IndexBuffer quad_index_buffer = IndexBuffer(quad_indices);
	data_.quad_.array_.SetIndexBuffer(quad_index_buffer);

	// Circles
	// data_.circle_.array_ = VertexArray::Create();

	// data_.circle_.buffer_ = VertexBuffer::Create(data_.max_vertices_ * sizeof(CircleVertex));
	// data_.circle_.buffer_.SetLayout({
	//	{glsl::Float3, "a_WorldPosition"},
	//	{glsl::Float3, "a_LocalPosition"},
	//	{glsl::Float4,			"a_Color"},
	//	{ glsl::Float,	   "a_Thickness"},
	//	{ glsl::Float,		   "a_Fade"},
	//	{	  glsl::Int,		 "a_EntityID"}
	// });
	// data_.circle_.array_.AddVertexBuffer(data_.circle_.buffer_);
	// data_.circle_.array_.SetIndexBuffer(quad_index_buffer); // Use quad IB
	// data_.circle_.buffer_base_ = new CircleVertex[data_.max_vertices_];

	// Lines
	/*data_.LineVertexArray = VertexArray::Create();

	data_.LineVertexBuffer = VertexBuffer::Create(data_.max_vertices_ * sizeof(LineVertex));
	data_.LineVertexBuffer.SetLayout({
		{glsl::Float3, "a_Position"},
		{glsl::Float4,	"a_Color"},
		 {   glsl::Int, "a_EntityID"}
	 });
	data_.LineVertexArray.AddVertexBuffer(data_.LineVertexBuffer);
	data_.line_.buffer_base_ = new LineVertex[data_.max_vertices_];*/

	// Text
	/*data_.TextVertexArray = VertexArray::Create();

	data_.TextVertexBuffer = VertexBuffer::Create(data_.max_vertices_ * sizeof(TextVertex));
	data_.TextVertexBuffer.SetLayout({
		{glsl::Float3, "a_Position"},
		{glsl::Float4,	"a_Color"},
		{glsl::Float2, "a_TexCoord"},
		{	  glsl::Int, "a_EntityID"}
	});
	data_.TextVertexArray.AddVertexBuffer(data_.TextVertexBuffer);
	data_.TextVertexArray.SetIndexBuffer(quad_index_buffer);
	data_.text_.buffer_base_ = new TextVertex[data_.max_vertices_];*/

	std::uint32_t whiteTextureData = 0xffffffff;
	data_.white_texture_		   = Texture(&whiteTextureData, { 1, 1 }, ImageFormat::RGBA8888);

	data_.max_texture_slots_ = GLRenderer::GetMaxTextureSlots();

	/*std::int32_t samplers[data_.max_texture_slots_];
	for (std::uint32_t i = 0; i < data_.max_texture_slots_; i++) {
		samplers[i] = i;
	}*/

	data_.quad_.shader_ = Shader(
		"resources/shader/renderer_quad_vertex.glsl", "resources/shader/renderer_quad_fragment.glsl"
	);
	/*data_.circle_.shader_ = Shader::Create("assets/shaders/Renderer2D_Circle.glsl");
	data_.line_.shader_	  = Shader::Create("assets/shaders/Renderer2D_Line.glsl");
	data_.text_.shader_	  = Shader::Create("assets/shaders/Renderer2D_Text.glsl");*/

	// Set first texture slot to 0.
	data_.texture_slots_.resize(data_.max_texture_slots_);
	data_.texture_slots_[0] = data_.white_texture_;

	data_.quad_vertex_positions_[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
	data_.quad_vertex_positions_[1] = { 0.5f, -0.5f, 0.0f, 1.0f };
	data_.quad_vertex_positions_[2] = { 0.5f, 0.5f, 0.0f, 1.0f };
	data_.quad_vertex_positions_[3] = { -0.5f, 0.5f, 0.0f, 1.0f };

	// TODO: Decide if necessary.
	// data_.CameraUniformBuffer = UniformBuffer::Create(sizeof(data_.CameraData), 0);

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
	data_.quad_.index_count_ = 0;
	data_.quad_.buffer_ptr_	 = data_.quad_.buffer_base_.data();

	/*data_.circle_.index_count_ = 0;
	data_.circle_.buffer_ptr_  = data_.circle_.buffer_base_;

	data_.line_.vertex_count_ = 0;
	data_.line_.buffer_ptr_	  = data_.line_.buffer_base_;

	data_.text_.index_count_ = 0;
	data_.text_.buffer_ptr_	 = data_.text_.buffer_base_;*/

	data_.texture_slot_index_ = 1;
}

void Renderer::Flush() {
	if (data_.quad_.index_count_) {
		PTGN_ASSERT(data_.quad_.buffer_ptr_ != nullptr);
		std::uint32_t dataSize = (std::uint32_t)(
			(uint8_t*)data_.quad_.buffer_ptr_ - (uint8_t*)data_.quad_.buffer_base_.data()
		);
		data_.quad_.buffer_.SetData(data_.quad_.buffer_base_.data(), dataSize);

		// Bind textures
		for (std::uint32_t i = 0; i < data_.texture_slot_index_; i++) {
			data_.texture_slots_[i].Bind(i);
		}

		data_.quad_.shader_.Bind();
		GLRenderer::DrawElements(data_.quad_.array_, data_.quad_.index_count_);
		// data_.stats.DrawCalls++;
	}

	/*if (data_.circle_.index_count_) {
		std::uint32_t dataSize =
			(std::uint32_t)((uint8_t*)data_.circle_.buffer_ptr_ -
	(uint8_t*)data_.circle_.buffer_base_); data_.circle_.buffer_.SetData(data_.circle_.buffer_base_,
	dataSize);

		data_.CircleShader.Bind();
		GLRenderer::DrawIndexed(data_.circle_.array_, data_.circle_.index_count_);
		data_.Stats.DrawCalls++;
	}*/

	/*if (data_.line_.vertex_count_) {
		std::uint32_t dataSize =
			(std::uint32_t)((uint8_t*)data_.line_.buffer_ptr_ - (uint8_t*)data_.line_.buffer_base_);
		data_.LineVertexBuffer.SetData(data_.line_.buffer_base_, dataSize);

		data_.LineShader.Bind();
		GLRenderer::SetLineWidth(data_.LineWidth);
		GLRenderer::DrawLines(data_.LineVertexArray, data_.line_.vertex_count_);
		data_.Stats.DrawCalls++;
	}*/

	/*if (data_.text_.index_count_) {
		std::uint32_t dataSize =
			(std::uint32_t)((uint8_t*)data_.text_.buffer_ptr_ - (uint8_t*)data_.text_.buffer_base_);
		data_.TextVertexBuffer.SetData(data_.text_.buffer_base_, dataSize);

		auto buf = data_.text_.buffer_base_;
		data_.FontAtlasTexture.Bind(0);

		data_.TextShader.Bind();
		GLRenderer::DrawIndexed(data_.TextVertexArray, data_.text_.index_count_);
		data_.Stats.DrawCalls++;
	}*/
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
	const V2_float& position, const V2_float& size, const Texture& texture, float tilingFactor,
	const V4_float& tintColor
) {
	DrawQuad({ position.x, position.y, 0.0f }, size, texture, tilingFactor, tintColor);
}

void Renderer::DrawQuad(
	const V3_float& position, const V2_float& size, const Texture& texture, float tilingFactor,
	const V4_float& tintColor
) {
	M4_float transform = M4_float::Translate(M4_float(1.0f), position) *
						 M4_float::Scale(M4_float(1.0f), { size.x, size.y, 1.0f });

	DrawQuad(transform, texture, tilingFactor, tintColor);
}

void Renderer::DrawQuad(const M4_float& transform, const V4_float& color) {
	constexpr size_t quad_vertex_count = 4;
	const float textureIndex		   = 0.0f; // White Texture
	constexpr V2_float textureCoords[] = {
		{0.0f, 0.0f},
		{1.0f, 0.0f},
		{1.0f, 1.0f},
		{0.0f, 1.0f}
	};
	const float tilingFactor = 1.0f;

	if (data_.quad_.index_count_ >= data_.max_indices_) {
		NextBatch();
	}

	for (size_t i = 0; i < quad_vertex_count; i++) {
		auto pos = transform * data_.quad_vertex_positions_[i];
		PTGN_ASSERT(data_.quad_.buffer_ptr_ != nullptr);
		data_.quad_.buffer_ptr_->position	   = { pos.x, pos.y, pos.z };
		data_.quad_.buffer_ptr_->color		   = { color.x, color.y, color.z, color.w };
		data_.quad_.buffer_ptr_->tex_coord	   = { textureCoords[i].x, textureCoords[i].y };
		data_.quad_.buffer_ptr_->tex_index	   = { textureIndex };
		data_.quad_.buffer_ptr_->tiling_factor = { tilingFactor };
		data_.quad_.buffer_ptr_++;
	}

	data_.quad_.index_count_ += 6;

	// data_.Stats.QuadCount++;
}

void Renderer::DrawQuad(
	const M4_float& transform, const Texture& texture, float tilingFactor, const V4_float& tintColor
) {
	constexpr size_t quad_vertex_count = 4;
	constexpr V2_float textureCoords[] = {
		{0.0f, 0.0f},
		{1.0f, 0.0f},
		{1.0f, 1.0f},
		{0.0f, 1.0f}
	};

	if (data_.quad_.index_count_ >= data_.max_indices_) {
		NextBatch();
	}

	float textureIndex = 0.0f;
	for (std::uint32_t i = 1; i < data_.texture_slot_index_; i++) {
		if (data_.texture_slots_[i].GetInstance() == texture.GetInstance()) {
			textureIndex = (float)i;
			break;
		}
	}

	if (textureIndex == 0.0f) {
		if (data_.texture_slot_index_ >= data_.max_texture_slots_) {
			NextBatch();
		}

		textureIndex									= (float)data_.texture_slot_index_;
		data_.texture_slots_[data_.texture_slot_index_] = texture;
		data_.texture_slot_index_++;
	}

	for (size_t i = 0; i < quad_vertex_count; i++) {
		auto pos						   = transform * data_.quad_vertex_positions_[i];
		data_.quad_.buffer_ptr_->position  = { pos.x, pos.y, pos.z };
		data_.quad_.buffer_ptr_->color	   = { tintColor.x, tintColor.y, tintColor.z, tintColor.w };
		data_.quad_.buffer_ptr_->tex_coord = { textureCoords[i].x, textureCoords[i].y };
		data_.quad_.buffer_ptr_->tex_index = { textureIndex };
		data_.quad_.buffer_ptr_->tiling_factor = { tilingFactor };
		data_.quad_.buffer_ptr_++;
	}

	data_.quad_.index_count_ += 6;

	// data_.Stats.QuadCount++;
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

// void Renderer::DrawCircle(
//	const M4_float& transform, const V4_float& color, float thickness /*= 1.0f*/,
//	float fade /*= 0.005f*/, int entityID /*= -1*/
//) {
//	// TODO: implement for circles
//	// if (data_.quad_.index_count_ >= data_.max_indices_)
//	// 	NextBatch();
//
//	for (size_t i = 0; i < 4; i++) {
//		data_.circle_.buffer_ptr_.WorldPosition = transform * data_.quad_vertex_positions_[i];
//		data_.circle_.buffer_ptr_.LocalPosition = data_.quad_vertex_positions_[i] * 2.0f;
//		data_.circle_.buffer_ptr_.Color			= color;
//		data_.circle_.buffer_ptr_.Thickness		= thickness;
//		data_.circle_.buffer_ptr_.Fade			= fade;
//		data_.circle_.buffer_ptr_.EntityID		= entityID;
//		data_.circle_.buffer_ptr_++;
//	}
//
//	data_.circle_.index_count_ += 6;
//
//	data_.Stats.QuadCount++;
// }
//
// void Renderer::DrawLine(const V3_float& p0, V3_float& p1, const V4_float& color, int entityID) {
//	data_.line_.buffer_ptr_.Position = p0;
//	data_.line_.buffer_ptr_.Color	 = color;
//	data_.line_.buffer_ptr_.EntityID = entityID;
//	data_.line_.buffer_ptr_++;
//
//	data_.line_.buffer_ptr_.Position = p1;
//	data_.line_.buffer_ptr_.Color	 = color;
//	data_.line_.buffer_ptr_.EntityID = entityID;
//	data_.line_.buffer_ptr_++;
//
//	data_.line_.vertex_count_ += 2;
// }

// void Renderer::DrawRect(const V3_float& position, const V2_float& size, const V4_float& color) {
//	V3_float p0 = V3_float(position.x - size.x * 0.5f, position.y - size.y * 0.5f, position.z);
//	V3_float p1 = V3_float(position.x + size.x * 0.5f, position.y - size.y * 0.5f, position.z);
//	V3_float p2 = V3_float(position.x + size.x * 0.5f, position.y + size.y * 0.5f, position.z);
//	V3_float p3 = V3_float(position.x - size.x * 0.5f, position.y + size.y * 0.5f, position.z);
//
//	DrawLine(p0, p1, color);
//	DrawLine(p1, p2, color);
//	DrawLine(p2, p3, color);
//	DrawLine(p3, p0, color);
// }

// void Renderer::DrawRect(const M4_float& transform, const V4_float& color) {
//	V3_float lineVertices[4];
//	for (size_t i = 0; i < 4; i++) {
//		auto pos		= transform * data_.quad_vertex_positions_[i];
//		lineVertices[i] = { pos.x, pos.y, pos.z };
//	}
//
//	DrawLine(lineVertices[0], lineVertices[1], color);
//	DrawLine(lineVertices[1], lineVertices[2], color);
//	DrawLine(lineVertices[2], lineVertices[3], color);
//	DrawLine(lineVertices[3], lineVertices[0], color);
// }

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
// void Renderer::ResetStats() {
//	memset(&data_.Stats, 0, sizeof(Statistics));
// }
//
// Renderer::Statistics Renderer::GetStats() {
//	return data_.Stats;
// }

} // namespace ptgn