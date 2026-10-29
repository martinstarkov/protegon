#pragma once

#include <string>
#include <string_view>

#include "core/util/file.h"
#include "renderer/text/font_atlas.h"
#include "renderer/text/text_style.h"
#include "runtime/asset/asset_key.h"
#include "runtime/graphics/text/font.h"

namespace ptgn {

/// @brief Default engine font key.
/// Do not modify this value.
/// Use ctx().font.SetDefault(new_default_font_key); to change the default font
inline constexpr std::string_view kDefaultFont{ "" };

class AssetManager;
class Renderer;

namespace impl {

class ApplicationContext;

} // namespace impl

class FontSystem {
public:
	Font GetDefault() const;

	void SetDefault(FontKey font_key = kDefaultFont);

private:
	friend class Shader;
	friend class Texture;
	friend class AssetManager;
	friend class impl::ApplicationContext;

	explicit FontSystem(Renderer& renderer, AssetManager& asset_manager);
	~FontSystem() noexcept;
	FontSystem(const FontSystem&)                 = delete;
	FontSystem& operator=(const FontSystem&)      = delete;
	FontSystem(FontSystem&&) noexcept             = delete;
	FontSystem& operator=(FontSystem&&) noexcept  = delete;

	[[nodiscard]] static impl::FontAtlasData PrepareFontAtlas(const path& font_path);
	[[nodiscard]] static impl::FontAtlas CreateFontAtlas(Renderer& renderer, const path& font_path);

	AssetManager& asset_manager_;

	FontKey default_font_{ kDefaultFont };
};

} // namespace ptgn
