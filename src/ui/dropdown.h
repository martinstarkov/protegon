#pragma once

#include <string>
#include <string_view>

#include "core/manager.h"
#include "renderer/text.h"
#include "ui/button.h"
#include "utility/type_traits.h"

namespace ptgn {

class Dropdown : public MapManager<Text, std::string_view, std::string, false> {
public:
	Dropdown()								 = default;
	virtual ~Dropdown()						 = default;
	Dropdown(Dropdown&&) noexcept			 = default;
	Dropdown& operator=(Dropdown&&) noexcept = default;
	Dropdown(const Dropdown&)				 = delete;
	Dropdown& operator=(const Dropdown&)	 = delete;

	template <typename TKey, typename... TArgs, tt::constructible<Text, TArgs...> = true>
	Text& Load(const TKey& key, TArgs&&... constructor_args) {
		auto k{ GetInternalKey(key) };
		Text& text{ MapManager::Load(key, Text{ std::forward<TArgs>(constructor_args)... }) };
		Button button;
		// button.Set<ButtonProperty::
		buttons_.Load(key, button);
		return text;
	}

	void Show() {
		ForEachValue([](Button& b) { b.Set<ButtonProperty::Toggled>(false); });
	}

	void Hide() {}

	void Draw() const;

private:
	MapManager<Button, std::string_view, std::string, false> buttons_;
};

} // namespace ptgn