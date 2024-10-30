#pragma once

#include "math/matrix4.h"
#include "renderer/frame_buffer.h"
#include "renderer/shader.h"
#include "renderer/texture.h"
#include "renderer/vertex_array.h"
#include "scene/camera.h"

namespace ptgn {

struct Rect;

class RenderTexture : public Texture {
public:
	// TODO: Somehow resize/refresh these on window resize.
	// Perhaps check if *this == game.draw.GetTarget() and then reset it?

	RenderTexture()			  = default;
	~RenderTexture() override = default;
	explicit RenderTexture(const Shader& shader);
	explicit RenderTexture(ScreenShader screen_shader);

	[[nodiscard]] const Shader& GetShader() const;

	void Draw();

	// TODO: Move to private.
	void Bind() const;

	// TODO: Move to private.
	[[nodiscard]] const VertexArray& GetVertexArray() const;

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	[[nodiscard]] BlendMode GetBlendMode() const;
	void SetBlendMode(BlendMode blend_mode);

	OrthographicCamera camera;

private:
	friend class impl::Renderer;

	Color clear_color_{ color::Transparent };
	BlendMode blend_mode_{ BlendMode::Add };
	Shader shader_;
	VertexArray vertex_array_;
	FrameBuffer frame_buffer_;
};

} // namespace ptgn