#pragma once

#include <functional>
#include <optional>

#include "core/assert.h"

namespace ptgn::impl::gl {

class GLContext;

template <typename T>
class BindGuard {
public:
	BindGuard(GLContext& gl, std::optional<T> id, bool restore_bind) :
		gl_{ gl },
		id_{ std::invoke([&id, restore_bind]() {
			if (restore_bind) {
				PTGN_ASSERT(id.has_value(), "Cannot restore bind without a previous bind");
			}
			if (!id.has_value()) {
				return T{ 0 };
			}
			return *id;
		}) },
		restore_bind_{ restore_bind } {}

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