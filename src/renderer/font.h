#pragma once

#include <cstdint>
#include <memory>
#include <string>

#include "components/generic.h"
#include "math/vector2.h"
#include "resources/fonts.h"
#include "resources/resource_manager.h"
#include "serialization/enum.h"
#include "serialization/fwd.h"
#include "utility/file.h"

#ifdef __EMSCRIPTEN__
struct _TTF_Font;
using TTF_Font = _TTF_Font;
#else
struct TTF_Font;
#endif
struct SDL_RWops;

namespace ptgn {

class Text;
class Scene;
class Camera;

static constexpr std::int32_t default_font_size{ 18 };
static constexpr std::int32_t default_font_index{ 0 };

enum class FontRenderMode : int {
	Blended = 0,
	Solid	= 1,
	Shaded	= 2
};

enum class FontStyle : int {
	Normal		  = 0, // TTF_STYLE_NORMAL
	Bold		  = 1, // TTF_STYLE_BOLD
	Italic		  = 2, // TTF_STYLE_ITALIC
	Underline	  = 4, // TTF_STYLE_UNDERLINE
	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
};

struct FontSize : public ArithmeticComponent<std::int32_t> {
	using ArithmeticComponent::ArithmeticComponent;

	FontSize() : ArithmeticComponent{ default_font_size } {}

	[[nodiscard]] FontSize GetHD(const Scene& scene, const Camera& camera) const;
};

[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
}

namespace impl {

class Game;

struct TTF_FontDeleter {
	void operator()(TTF_Font* font) const;
};

using Font = std::unique_ptr<TTF_Font, TTF_FontDeleter>;

using TemporaryFont = std::shared_ptr<TTF_Font>;

class FontManager : public ResourceManager<FontManager, ResourceHandle, Font> {
public:
	FontManager()							   = default;
	FontManager(const FontManager&)			   = delete;
	FontManager& operator=(const FontManager&) = delete;
	FontManager(FontManager&& other) noexcept;
	FontManager& operator=(FontManager&& other) noexcept;
	~FontManager() override;

	void Load(const ResourceHandle& key, const path& filepath) final;

	const Font& Get(const ResourceHandle& key) const = delete;

	void Load(
		const ResourceHandle& key, const path& filepath, std::int32_t size,
		std::int32_t index = default_font_index
	);

	void Load(
		const ResourceHandle& key, const FontBinary& binary, std::int32_t size = default_font_size,
		std::int32_t index = default_font_index
	);

	// Empty font key corresponds to the engine default font.
	void SetDefault(const ResourceHandle& key = {});

	[[nodiscard]] int GetLineSkip(const ResourceHandle& key, const FontSize& font_size = {}) const;

	// @param font_size If left with default {}, will use currently set font size of the provided
	// font key.
	[[nodiscard]] V2_int GetSize(
		const ResourceHandle& key, const std::string& content, const FontSize& font_size = {}
	) const;

	// @return Total height of the font in pixels.
	[[nodiscard]] FontSize GetHeight(const ResourceHandle& key, const FontSize& font_size = {})
		const;

	// Note: This function will not serialize any fonts loaded from binaries.
	friend void to_json(json& j, const FontManager& manager);

	// Note: This function will not deserialize any fonts loaded from binaries.
	friend void from_json(const json& j, FontManager& manager);

private:
	friend class Game;
	friend class ptgn::Text;
	friend ParentManager;

	// Initializes the default font from a binary.
	void Init();

	[[nodiscard]] static SDL_RWops* GetRawBuffer(const FontBinary& binary);

	// @param free_buffer If true, frees raw_buffer after use.
	[[nodiscard]] static TTF_Font* LoadFromBinary(
		SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
	);

	[[nodiscard]] static Font LoadFromBinary(
		const FontBinary& binary, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] static Font LoadFromFile(
		const path& filepath, std::int32_t size, std::int32_t index
	);

	[[nodiscard]] static Font LoadFromFile(const path& filepath);

	[[nodiscard]] TemporaryFont Get(const ResourceHandle& key, const FontSize& font_size = {})
		const;

	ResourceHandle default_key_;

	SDL_RWops* raw_default_font_{ nullptr };
};

} // namespace impl

PTGN_SERIALIZER_REGISTER_ENUM(
	FontRenderMode, { { FontRenderMode::Solid, "solid" },
					  { FontRenderMode::Shaded, "shaded" },
					  { FontRenderMode::Blended, "blended" } }
);

PTGN_SERIALIZER_REGISTER_ENUM(
	FontStyle, { { FontStyle::Normal, "normal" },
				 { FontStyle::Bold, "bold" },
				 { FontStyle::Italic, "italic" },
				 { FontStyle::Underline, "underline" },
				 { FontStyle::Strikethrough, "strikethrough" } }
);

} // namespace ptgn