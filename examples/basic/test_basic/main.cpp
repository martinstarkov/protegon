#include <cstdint>
#include <functional>
#include <iostream>
#include <string>
#include <unordered_map>

// ============================================================
// Assume ECS Exists
// ============================================================

struct Scene;
struct App;

struct Entity {
	template <typename T>
	T& Get();

	template <typename T>
	bool Has() const;

	template <typename T, typename... Args>
	T& Add(Args&&...);

	void Destroy();

	Scene& GetScene();
};

// ============================================================
// Backend Example: OpenGL
// ============================================================

class OpenGLBackend {
public:
	using TextureHandle = uint32_t;

	TextureHandle CreateTexture(int width, int height) {
		static uint32_t next = 1;
		TextureHandle id	 = next++;
		std::cout << "[GL] CreateTexture id=" << id << " size=" << width << "x" << height << "\n";
		return id;
	}

	void DestroyTexture(TextureHandle id) {
		std::cout << "[GL] DestroyTexture id=" << id << "\n";
	}

	void DrawTexture(TextureHandle id) {
		if (m_boundTexture != id) {
			std::cout << "[GL] BindTexture id=" << id << "\n";
			m_boundTexture = id;
		}

		std::cout << "[GL] Draw call\n";
	}

private:
	TextureHandle m_boundTexture = 0; // state cache
};

// ============================================================
// Renderer (Template, Backend-Agnostic)
// ============================================================

template <typename Backend>
class Renderer {
public:
	using TextureHandle = typename Backend::TextureHandle;

	TextureHandle CreateTexture(int width, int height) {
		return m_backend.CreateTexture(width, height);
	}

	void DestroyTexture(TextureHandle handle) {
		m_backend.DestroyTexture(handle);
	}

	void DrawTexture(TextureHandle handle) {
		m_backend.DrawTexture(handle);
	}

private:
	Backend m_backend;
};

// ============================================================
// App / Scene access
// ============================================================

using ActiveBackend	 = OpenGLBackend;
using ActiveRenderer = Renderer<ActiveBackend>;

struct App {
	ActiveRenderer renderer;
};

struct Scene {
	App& app();
};

// ============================================================
// Resource Components
// ============================================================

struct TextureGPU {
	ActiveRenderer::TextureHandle handle;
};

struct TextureSize {
	int width;
	int height;
};

struct RefCount {
	uint32_t value = 0;
};

struct PersistentTag {};

// TODO: Add name component for debugging purposes?

struct ResourceKey {
	uint64_t hash;
};

// ============================================================
// ResourceHandle
// ============================================================

template <typename Tag>
class ResourceHandle {
public:
	ResourceHandle() = default;

	explicit ResourceHandle(Entity e) : m_entity(e) {
		AddRef();
	}

	ResourceHandle(const ResourceHandle& other) : m_entity(other.m_entity) {
		AddRef();
	}

	ResourceHandle(ResourceHandle&& other) noexcept : m_entity(other.m_entity) {
		other.m_entity = {};
	}

	~ResourceHandle() {
		Release();
	}

	Entity GetEntity() const {
		return m_entity;
	}

	bool Valid() const {
		return m_entity.Has<RefCount>();
	}

private:
	void AddRef() {
		if (!Valid()) {
			return;
		}
		m_entity.Get<RefCount>().value++;
	}

	void Release() {
		if (!Valid()) {
			return;
		}

		auto& rc = m_entity.Get<RefCount>();

		if (--rc.value == 0) {
			if (m_entity.Has<PersistentTag>()) {
				return;
			}

			ResourceTraits<Tag>::Destroy(m_entity);
		}
	}

	Entity m_entity{};
};

template <typename Tag>
struct ResourceTraits;

// Alias
struct TextureTag {};

template <>
struct ResourceTraits<TextureTag> {
	static void Destroy(Entity entity) {
		auto& gpu	   = entity.Get<TextureGPU>();
		auto& renderer = entity.GetScene().app().renderer;

		renderer.DestroyTexture(gpu.handle);
		entity.Destroy();
	}
};

using Texture = ResourceHandle<TextureTag>;

struct ShaderTag {};

// template <>
// struct ResourceTraits<ShaderTag> {
//	static void Destroy(Entity entity) {
//		auto& gpu	   = entity.Get<ShaderGPU>();
//		auto& renderer = entity.GetScene().app().renderer;
//
//		renderer.DestroyShader(gpu.handle);
//		entity.Destroy();
//	}
// };

template <typename Derived>
class RefCountedResource {
public:
	RefCountedResource() = default;

	explicit RefCountedResource(Entity e) : m_entity(e) {
		AddRef();
	}

	RefCountedResource(const RefCountedResource& other) : m_entity(other.m_entity) {
		AddRef();
	}

	RefCountedResource(RefCountedResource&& other) noexcept : m_entity(other.m_entity) {
		other.m_entity = {};
	}

	RefCountedResource& operator=(const RefCountedResource& other) {
		if (this != &other) {
			Release();
			m_entity = other.m_entity;
			AddRef();
		}
		return *this;
	}

	RefCountedResource& operator=(RefCountedResource&& other) noexcept {
		if (this != &other) {
			Release();
			m_entity	   = other.m_entity;
			other.m_entity = {};
		}
		return *this;
	}

	~RefCountedResource() {
		Release();
	}

protected:
	Entity m_entity{};

private:
	void AddRef() {
		if (!Valid()) {
			return;
		}
		m_entity.Get<RefCount>().value++;
	}

	void Release() {
		if (!Valid()) {
			return;
		}

		auto& rc = m_entity.Get<RefCount>();

		if (--rc.value == 0) {
			if (!m_entity.Has<PersistentTag>()) {
				static_cast<Derived*>(this)->Destroy();
			}
		}
	}

	bool Valid() const {
		return m_entity.Has<RefCount>();
	}
};

class TextureTest : public RefCountedResource<TextureTest> {
public:
	using RefCountedResource::RefCountedResource;

	void Destroy() {
		auto& gpu	   = m_entity.Get<TextureGPU>();
		auto& renderer = m_entity.GetScene().app().renderer;

		renderer.DestroyTexture(gpu.handle);
		m_entity.Destroy();
	}
};

// ============================================================
// AssetManager
// ============================================================

class AssetManager {
public:
	explicit AssetManager(Scene& scene) : m_scene(scene) {}

	Texture LoadTexture(int width, int height) {
		Entity e = CreateResourceEntity();

		auto& renderer = m_scene.app().renderer;

		auto handle = renderer.CreateTexture(width, height);

		e.Add<TextureGPU>().handle = handle;
		e.Add<TextureSize>()	   = { width, height };
		e.Add<RefCount>();

		return Texture(e);
	}

	Texture LoadTexture(std::string_view key, int width, int height) {
		uint64_t hash = Hash(key);

		if (auto it = m_keyLookup.find(hash); it != m_keyLookup.end()) {
			return Texture(it->second);
		}

		Entity e = CreateResourceEntity();

		auto& renderer = m_scene.app().renderer;

		auto handle = renderer.CreateTexture(width, height);

		e.Add<TextureGPU>().handle = handle;
		e.Add<TextureSize>()	   = { width, height };
		e.Add<RefCount>();
		e.Add<PersistentTag>();
		e.Add<ResourceKey>().hash = hash;

		m_keyLookup[hash] = e;

		return Texture(e);
	}

	void Unload(std::string_view key) {
		uint64_t hash = Hash(key);

		auto it = m_keyLookup.find(hash);
		if (it == m_keyLookup.end()) {
			return;
		}

		Entity e = it->second;

		auto& rc = e.Get<RefCount>();

		if (rc.value == 0) {
			auto& gpu = e.Get<TextureGPU>();
			m_scene.app().renderer.DestroyTexture(gpu.handle);
			e.Destroy();
		} else {
			e.Destroy(); // remove persistence
		}

		m_keyLookup.erase(it);
	}

private:
	Entity CreateResourceEntity() {
		extern Entity CreateEntity(Scene&);
		return CreateEntity(m_scene);
	}

	static uint64_t Hash(std::string_view str) {
		return std::hash<std::string_view>{}(str);
	}

	Scene& m_scene;
	std::unordered_map<uint64_t, Entity> m_keyLookup;
};

// ============================================================
// Example Usage (conceptual)
// ============================================================

void Example(Scene& scene) {
	AssetManager assets(scene);

	{
		Texture tex = assets.LoadTexture(256, 256);

		auto& size = tex.GetEntity().Get<TextureSize>();

		std::cout << "Size: " << size.width << "x" << size.height << "\n";

		scene.app().renderer.DrawTexture(tex.GetEntity().Get<TextureGPU>().handle);
	} // auto destroyed (non-persistent)

	Texture ui = assets.LoadTexture("ui", 512, 512);

	ui = {};			 // refcount 0 but persistent survives

	assets.Unload("ui"); // destroyed now
}
