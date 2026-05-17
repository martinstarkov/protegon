#pragma once

namespace ptgn {

namespace impl {

class Renderer;

} // namespace impl

class DrawContext {
public:
private:
	explicit DrawContext(impl::Renderer& renderer);

	impl::Renderer& renderer_;
};

} // namespace ptgn