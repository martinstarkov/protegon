#pragma once

#include <memory>
#include <type_traits>

#include "core/manager.h"
#include "protegon/button.h"
#include "utility/type_traits.h"

namespace ptgn {

class Game;
class UserInterface;

using ButtonKey = std::size_t;

class ButtonManager : public Manager<std::shared_ptr<Button>> {
private:
	ButtonManager()									   = default;
	~ButtonManager() override						   = default;
	ButtonManager(const ButtonManager&)				   = delete;
	ButtonManager(ButtonManager&&) noexcept			   = default;
	ButtonManager& operator=(const ButtonManager&)	   = delete;
	ButtonManager& operator=(ButtonManager&&) noexcept = default;

public:
	template <typename T, tt::convertible<T*, Button*> = true>
	std::shared_ptr<Button> Load(ButtonKey button_key, T&& button) {
		auto button_ptr{ Manager::Load(button_key, std::make_shared<T>(std::move(button))) };
		button_ptr->SubscribeToMouseEvents();
		return button_ptr;
	}

	template <typename TButton = Button>
	[[nodiscard]] std::shared_ptr<TButton> Get(ButtonKey button_key) {
		static_assert(
			std::is_base_of_v<Button, TButton> || std::is_same_v<TButton, Button>,
			"Cannot cast retrieved button to type which does not inherit from the Button class"
		);
		return std::static_pointer_cast<TButton>(Manager::Get(button_key));
	}

	void DrawFilled(ButtonKey button_key) const;
	void DrawHollow(ButtonKey button_key, float line_width) const;
	void DrawAllFilled() const;
	void DrawAllHollow(float line_width) const;

	// TODO: Figure out button click crash.

	// void Unload(ButtonKey button_key);

	/*void Reset() {
		flagged_ = {};
		Manager::Reset();
	}*/

	/*void Clear() {

	}*/

private:
	std::vector<ButtonKey> flagged_;

	friend class Game;
	friend class UserInterface;
};

class UserInterface {
private:
	UserInterface()									   = default;
	~UserInterface()								   = default;
	UserInterface(const UserInterface&)				   = delete;
	UserInterface(UserInterface&&) noexcept			   = default;
	UserInterface& operator=(const UserInterface&)	   = delete;
	UserInterface& operator=(UserInterface&&) noexcept = default;

public:
	ButtonManager button;

private:
	friend class Game;
};

} // namespace ptgn