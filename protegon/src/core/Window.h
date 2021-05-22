#pragma once

#include <cstdint> // std::uint32_t

#include "math/Vector2.h"
#include "utils/Singleton.h"

struct SDL_Window;

namespace engine {

class Window : public Singleton<Window> {
public:
	static V2_int GetSize();
	static V2_int GetOriginPosition();
	static const char* GetTitle();

	static void SetSize(const V2_int& new_size);
	static void SetOriginPosition(const V2_int& new_origin);
	static void SetTitle(const char* new_title);
	static void SetFullscreen(bool on);
	static void SetResizeable(bool on);

	Window() = default;

	operator SDL_Window* () const;
	SDL_Window* operator&() const;
private:
	friend class Engine;
	
	static Window& Init(const char* title, 
						const V2_int& position, 
						const V2_int& size, 
						std::uint32_t flags = 0);
	static void Destroy();

	bool IsValid() const;

	SDL_Window* window_{ nullptr };
};

} // namespace engine