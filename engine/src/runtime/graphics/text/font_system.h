#pragma once

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "core/graphics/color.h"
#include "core/graphics/surface.h"
#include "core/math/vector2.h"
#include "core/util/file.h"
#include "runtime/graphics/text/font.h"
#include "runtime/graphics/text/text.h"

namespace ptgn {

class AssetManager;

namespace impl {

class Renderer;

} // namespace impl

class FontSystem {
public:
	explicit FontSystem(AssetManager& assets);
	~FontSystem() noexcept;
	FontSystem(const FontSystem&)				 = delete;
	FontSystem& operator=(const FontSystem&)	 = delete;
	FontSystem(FontSystem&&) noexcept			 = delete;
	FontSystem& operator=(FontSystem&&) noexcept = delete;

	Font GetDefault() const;

	/// @param font Default ({}) key corresponds to the engine default font.
	void SetDefault(std::string_view font_key = {});

	int GetLineSkip(std::string_view font_key, FontSize font_size = {}) const;

	/// @param text_content Text to calculate size of, in UTF-8 encoding.
	/// @param font_size Optional font size to check the size for. If {}, uses the current font
	/// size.
	/// @param max_wrap_width The maximum width or 0 to wrap on newline characters.
	V2_int GetSize(
		std::string_view font_key, std::string_view text_content, FontSize font_size = {},
		int max_wrap_width = 0
	) const;

	/// @param font_size Optional font size to check the height for. If {}, uses the current font
	/// size.
	int GetHeight(std::string_view font_key, FontSize font_size = {}) const;

private:
	friend class Shader;
	friend class Texture;
	friend class AssetManager;

	[[nodiscard]] static impl::FontObject CreateFont(
		impl::Renderer& renderer, const path& font_path, std::string_view name
	);

	AssetManager& assets_;

	std::string default_font_;
	void* raw_default_font_{ nullptr };
};

} // namespace ptgn