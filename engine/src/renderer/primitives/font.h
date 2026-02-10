// #pragma once
//
// #include <cstdint>
//
// #include "ecs/components/generic.h"
//
// namespace ptgn {
//
// class Text;
// class Scene;
// class Camera;
//
// static constexpr std::int32_t default_font_size{ 18 };
// static constexpr std::int32_t default_font_index{ 0 };
//
// enum class FontRenderMode : int {
//	Blended = 0,
//	Solid	= 1,
//	Shaded	= 2
// };
//
// enum class FontStyle : int {
//	Normal		  = 0, // TTF_STYLE_NORMAL
//	Bold		  = 1, // TTF_STYLE_BOLD
//	Italic		  = 2, // TTF_STYLE_ITALIC
//	Underline	  = 4, // TTF_STYLE_UNDERLINE
//	Strikethrough = 8  // TTF_STYLE_STRIKETHROUGH
// };
//
// struct FontSize : public ArithmeticComponent<std::int32_t> {
//	using ArithmeticComponent::ArithmeticComponent;
//
//	FontSize() : ArithmeticComponent{ default_font_size } {}
//
//	[[nodiscard]] FontSize GetHD(const Scene& scene, const Camera& camera) const;
// };
//
//[[nodiscard]] inline FontStyle operator&(FontStyle a, FontStyle b) {
//	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
// }
//
//[[nodiscard]] inline FontStyle operator|(FontStyle a, FontStyle b) {
//	return static_cast<FontStyle>(static_cast<int>(a) | static_cast<int>(b));
// }
//
// namespace impl {
//
//// Empty font key corresponds to the engine default font.
//// void SetDefaultFont(const ResourceHandle& key = {});
//
////[[nodiscard]] static SDL_RWops* GetRawBuffer(const FontBinary& binary);
//
//// @param free_buffer If true, frees raw_buffer after use.
////[[nodiscard]] static TTF_Font* LoadFromBinary(
////	SDL_RWops* raw_buffer, std::int32_t size, std::int32_t index, bool free_buffer
////);
//
////[[nodiscard]] static Font LoadFromBinary(
////	const FontBinary& binary, std::int32_t size, std::int32_t index
////);
//
////[[nodiscard]] static Font LoadFromFile(
////	const path& filepath, std::int32_t size, std::int32_t index
////);
//
////[[nodiscard]] static Font LoadFromFile(const path& filepath);
//
////[[nodiscard]] TemporaryFont Get(const ResourceHandle& key, const FontSize& font_size = {})
///const;
//
//// ResourceHandle default_key_;
//
//// SDL_RWops* raw_default_font_{ nullptr };
//
//} // namespace impl
//
//} // namespace ptgn