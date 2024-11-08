#pragma once

#include "math/vector2.h"
#include "renderer/color.h"
#include "renderer/frame_buffer.h"
#include "renderer/texture.h"

namespace ptgn {

struct Rect;

class RenderTexture {
public:
	RenderTexture()	 = default;
	~RenderTexture() = default;

	explicit RenderTexture(const V2_float& size, const Color& clear_color = color::Transparent);

	void Clear();

	[[nodiscard]] bool IsValid() const;
	bool operator==(const RenderTexture& o) const;
	bool operator!=(const RenderTexture& o) const;

	[[nodiscard]] Color GetClearColor() const;
	void SetClearColor(const Color& clear_color);

	[[nodiscard]] FrameBuffer GetFrameBuffer() const;
	[[nodiscard]] Texture GetTexture() const;

	void DrawAndUnbind(bool force_draw = false) const;
	void Bind() const;

private:
	friend class impl::Renderer;

	// Set to false by the renderer when any draw calls are flushed to the current render target.
	bool cleared_{ true };
	Texture texture_;
	Color clear_color_{ color::Transparent };
	FrameBuffer frame_buffer_;
};

} // namespace ptgn