#pragma once

namespace ptgn::impl {

class UserInterface {
public:
	UserInterface()									   = default;
	~UserInterface()								   = default;
	UserInterface(const UserInterface&)				   = delete;
	UserInterface(UserInterface&&) noexcept			   = default;
	UserInterface& operator=(const UserInterface&)	   = delete;
	UserInterface& operator=(UserInterface&&) noexcept = default;
};

} // namespace ptgn::impl