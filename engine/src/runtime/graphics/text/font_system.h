#pragma once

#include <string>
#include <string_view>

#include "core/util/file.h"
#include "runtime/graphics/text/font.h"

namespace ptgn {

class AssetManager;

namespace impl {

class ApplicationContext;

} // namespace impl

class FontSystem {
public:
	Font GetDefault() const;

	/// @param font Default ({}) key corresponds to the engine default font.
	void SetDefault(std::string_view font_key = {});

private:
	friend class Shader;
	friend class Texture;
	friend class AssetManager;
	friend class impl::ApplicationContext;

	explicit FontSystem(AssetManager& assets);
	~FontSystem() noexcept;
	FontSystem(const FontSystem&)				 = delete;
	FontSystem& operator=(const FontSystem&)	 = delete;
	FontSystem(FontSystem&&) noexcept			 = delete;
	FontSystem& operator=(FontSystem&&) noexcept = delete;

	[[nodiscard]] static impl::FontObject CreateFont(
		const AssetManager& asset_manager, const path& font_path
	);

	AssetManager& assets_;

	std::string default_font_;
};

} // namespace ptgn