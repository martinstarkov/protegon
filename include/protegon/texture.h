#pragma once

#include <cstdint>

#include "file.h"
#include "handle.h"
#include "surface.h"
#include "vector2.h"

namespace ptgn {

//
//	struct TextureSpecification
//	{
//		uint32_t Width = 1;
//		uint32_t Height = 1;
//		ImageFormat Format = ImageFormat::RGBA8;
//		bool GenerateMips = true;
//	};
//
//	class Texture
//	{
//	public:
//		virtual ~Texture() = default;
//
//		virtual const TextureSpecification& GetSpecification() const = 0;
//
//		virtual uint32_t GetWidth() const = 0;
//		virtual uint32_t GetHeight() const = 0;
//		virtual uint32_t GetRendererID() const = 0;
//
//		virtual const std::string& GetPath() const = 0;
//
//		virtual void SetData(void* data, uint32_t size) = 0;
//
//		virtual void Bind(uint32_t slot = 0) const = 0;
//
//		virtual bool IsLoaded() const = 0;
//
//		virtual bool operator==(const Texture& other) const = 0;
//	};
//
//	class Texture2D : public Texture
//	{
//	public:
//		static Ref<Texture2D> Create(const TextureSpecification& specification);
//		static Ref<Texture2D> Create(const std::string& path);
//	};

// class OpenGLTexture2D : public Texture2D
//	{
//	public:
//		OpenGLTexture2D(const TextureSpecification& specification);
//		OpenGLTexture2D(const std::string& path);
//		virtual ~OpenGLTexture2D();
//
//		virtual const TextureSpecification& GetSpecification() const override { return
// m_Specification; }
//
//		virtual uint32_t GetWidth() const override { return m_Width;  }
//		virtual uint32_t GetHeight() const override { return m_Height; }
//		virtual uint32_t GetRendererID() const override { return m_RendererID; }
//
//		virtual const std::string& GetPath() const override { return m_Path; }
//
//		virtual void SetData(void* data, uint32_t size) override;
//
//		virtual void Bind(uint32_t slot = 0) const override;
//
//		virtual bool IsLoaded() const override { return m_IsLoaded; }
//
//		virtual bool operator==(const Texture& other) const override
//		{
//			return m_RendererID == other.GetRendererID();
//		}
//	private:
//		TextureSpecification m_Specification;
//
//		std::string m_Path;
//		bool m_IsLoaded = false;
//		uint32_t m_Width, m_Height;
//		uint32_t m_RendererID;
//		GLenum m_InternalFormat, m_DataFormat;
//	};

namespace impl {

struct TextureInstance {
	TextureInstance(const Surface& surface);
	~TextureInstance();

	std::uint32_t id_{ 0 };
	V2_int size_;

	// TODO: Implement
	//void SetData(void* pixel_data, std::size_t size);

	struct GLFormats {
		std::int32_t internal_{ 0 };
		std::uint32_t format_{ 0 };
	};

	static GLFormats GetGLFormats(ImageFormat format);

};

} // namespace impl

class Texture : public Handle<impl::TextureInstance> {
public:
	Texture() = default;
	Texture(const Surface& surface);

	void Bind(std::uint32_t slot = 0) const;
	void Unbind() const;

	V2_int GetSize() const;
	// enum class AccessType : int {
	//	STATIC = 0,	   // SDL_TEXTUREACCESS_STATIC    /* Changes rarely, not
	//				   // lockable */
	//	STREAMING = 1, // SDL_TEXTUREACCESS_STREAMING /* Changes frequently,
	//				   // lockable */
	//	TARGET = 2,	   // SDL_TEXTUREACCESS_TARGET
	// };

	// Texture() = default;
	// Texture(const path& image_path);
	// Texture(AccessType access, const V2_int& size);

	//// Rotation in degrees. Positive clockwise.
	// void Draw(
	//	const Rectangle<float>& destination, const Rectangle<int>& source = {}, float angle = 0.0f,
	//	Flip flip = Flip::None, V2_int* center_of_rotation = nullptr
	//) const;

	//[[nodiscard]] V2_int GetSize() const;

	// void SetBlendMode(BlendMode mode);

	// void SetAlpha(std::uint8_t alpha);

	// void SetColor(const Color& color);

	// AccessType GetAccessType() const;

	// Texture(const std::shared_ptr<SDL_Surface>& surface);
};

} // namespace ptgn