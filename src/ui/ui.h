#pragma once

#include <memory>
#include <type_traits>

#include "core/manager.h"
#include "protegon/button.h"
#include "utility/type_traits.h"

namespace ptgn::impl {

class UserInterface;

class ButtonManager : public MapManager<std::shared_ptr<Button>> {
public:
	using MapManager::MapManager;

	template <typename TKey, typename T, tt::convertible<T*, Button*> = true>
	std::shared_ptr<Button> Load(const TKey& button_key, T&& button) {
		auto button_ptr{
			MapManager::Load(GetInternalKey(button_key), std::make_shared<T>(std::move(button)))
		};
		button_ptr->SubscribeToMouseEvents();
		return button_ptr;
	}

	template <typename TButton = Button, typename TKey = Key>
	[[nodiscard]] std::shared_ptr<TButton> Get(const TKey& button_key) {
		static_assert(
			std::is_base_of_v<Button, TButton> || std::is_same_v<TButton, Button>,
			"Cannot cast retrieved button to type which does not inherit from the Button class"
		);
		return std::static_pointer_cast<TButton>(MapManager::Get(GetInternalKey(button_key)));
	}

	template <typename TKey>
	void DrawFilled(const TKey& button_key) const {
		DrawFilledImpl(GetInternalKey(button_key));
	}

	template <typename TKey>
	void DrawHollow(const TKey& button_key, float line_width) const {
		DrawHollowImpl(GetInternalKey(button_key), line_width);
	}

	void DrawAllFilled() const;
	void DrawAllHollow(float line_width) const;

	using ForEachFunction = std::variant<
		std::function<void(InternalKey, std::shared_ptr<Button>)>,
		std::function<void(std::shared_ptr<Button>)>, std::function<void(InternalKey)>>;

	void ForEach(const ForEachFunction& function) {
		for (auto& [key, button] : GetMap()) {
			if (std::holds_alternative<std::function<void(std::shared_ptr<Button>)>>(function)) {
				std::invoke(
					std::get<std::function<void(std::shared_ptr<Button>)>>(function), button
				);
			} else if (std::holds_alternative<std::function<void(InternalKey)>>(function)) {
				std::invoke(std::get<std::function<void(InternalKey)>>(function), key);
			} else if (std::holds_alternative<
						   std::function<void(InternalKey, std::shared_ptr<Button>)>>(function)) {
				std::invoke(
					std::get<std::function<void(InternalKey, std::shared_ptr<Button>)>>(function),
					key, button
				);
			}
		}
	}

	// TODO: Figure out button click crash.

	// void Unload(ButtonKey button_key);

	/*void Reset() {
		flagged_ = {};
		MapManager::Reset();
	}*/

	/*void Clear() {

	}*/

private:
	void DrawFilledImpl(const InternalKey& button_key) const;
	void DrawHollowImpl(const InternalKey& button_key, float line_width) const;

	std::vector<InternalKey> flagged_;

	friend class UserInterface;
};

class UserInterface {
public:
	UserInterface()									   = default;
	~UserInterface()								   = default;
	UserInterface(const UserInterface&)				   = delete;
	UserInterface(UserInterface&&) noexcept			   = default;
	UserInterface& operator=(const UserInterface&)	   = delete;
	UserInterface& operator=(UserInterface&&) noexcept = default;

	ButtonManager button;
};

} // namespace ptgn::impl