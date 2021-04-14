#pragma once

#include <cstdint> // std::uint32_t
#include <cstdlib> // std::size_t

#include "math/Vector2.h"

struct SDL_Window;

namespace engine {

// Windows must be freed using Destroy().
class Window {
public:
	Window() = default;
	
	operator SDL_Window* () const;
	SDL_Window* operator&() const;
	
	bool IsValid() const;
	void Destroy();

	V2_int GetSize() const;
	V2_int GetPosition() const;
	const char* GetTitle() const;
	std::size_t GetDisplayIndex() const;

	void SetSize(const V2_int& new_size);
	void SetPosition(const V2_int& new_position);
	void SetTitle(const char* new_title);

private:
	friend class InputHandler;
	friend class Engine;

	Window(SDL_Window* window);
	Window(const char* title, const V2_int& position, const V2_int& size, std::size_t display_index, std::uint32_t flags = 0);

	SDL_Window* window_{ nullptr };
	std::size_t display_index_{ 0 };
};

} // namespace engine