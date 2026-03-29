#pragma once

namespace ptgn::impl::gl {

class GLContext;

template <typename T>
class BindGuard {
public:
	BindGuard(GLContext& gl, T id, bool restore_bind) :
		gl_{ gl }, id_{ id }, restore_bind_{ restore_bind } {}

	~BindGuard() noexcept;

	BindGuard(BindGuard&&) noexcept			   = delete;
	BindGuard& operator=(BindGuard&&) noexcept = delete;
	BindGuard(const BindGuard&)				   = delete;
	BindGuard& operator=(const BindGuard&)	   = delete;

private:
	GLContext& gl_;
	T id_;
	bool restore_bind_{ false };
};

} // namespace ptgn::impl::gl