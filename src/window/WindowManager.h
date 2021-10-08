#pragma once

#include <cstdint> // std::uint32_t

#include "utils/Defines.h"
#include "math/Vector2.h"

class SDL_Window;

namespace ptgn {

namespace interfaces {

class WindowManager {
public:
	virtual void CreateWindow(const char* title, const V2_int& size, const V2_int& position, std::uint32_t flags = 0) = 0;
	virtual void DestroyWindow() = 0;
	virtual V2_int GetWindowSize() const = 0;
	virtual V2_int GetWindowOriginPosition() const = 0;
	virtual const char* GetWindowTitle() const = 0;
	virtual void SetWindowSize(const V2_int& new_size) = 0;
	virtual void SetWindowOriginPosition(const V2_int& new_origin) = 0;
	virtual void SetWindowTitle(const char* new_title) = 0;
	virtual void SetWindowFullscreen(bool on) = 0;
	virtual void SetWindowResizeable(bool on) = 0;
};

} // namespace interfaces

namespace impl {

class SDLWindowManager : public interfaces::WindowManager {
public:
	SDLWindowManager();
    ~SDLWindowManager();
	virtual void CreateWindow(const char* title, const V2_int& size, const V2_int& position = window::CENTERED, std::uint32_t flags = 0) override;
	virtual void DestroyWindow() override;
	virtual V2_int GetWindowSize() const override;
	virtual V2_int GetWindowOriginPosition() const override;
	virtual const char* GetWindowTitle() const override;
	virtual void SetWindowSize(const V2_int& new_size) override;
	virtual void SetWindowOriginPosition(const V2_int& new_origin) override;
	virtual void SetWindowTitle(const char* new_title) override;
	virtual void SetWindowFullscreen(bool on) override;
	virtual void SetWindowResizeable(bool on) override;
	SDL_Window* window_{ nullptr };
};

SDLWindowManager& GetSDLWindowManager();

} // namespace impl

namespace services {

interfaces::WindowManager& GetWindowManager();

} // namespace services

} // namespace ptgn