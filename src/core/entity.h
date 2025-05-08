#pragma once

#include <array>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

#include "common/type_info.h"
#include "components/common.h"
#include "components/transform.h"
#include "components/uuid.h"
#include "ecs/ecs.h"
#include "math/hash.h"
#include "math/vector2.h"
#include "rendering/api/blend_mode.h"
#include "rendering/api/color.h"
#include "rendering/api/origin.h"
#include "serialization/fwd.h"

namespace ptgn {

// TODO: Move to different file.

// Forward declarations for parameter types
namespace impl {

class RenderData;

} // namespace impl

class Entity;

namespace tt {

namespace impl {

template <typename, typename = std::void_t<>>
struct has_static_draw : std::false_type {};

template <typename T>
struct has_static_draw<
	T, std::void_t<decltype(T::Draw(
		   std::declval<ptgn::impl::RenderData&>(), std::declval<const Entity&>()
	   ))>> :
	std::is_same<
		decltype(T::Draw(std::declval<ptgn::impl::RenderData&>(), std::declval<const Entity&>())),
		void> {};

} // namespace impl

// Trait to detect static void Draw(impl::RenderData&, const Entity&)
template <typename T>
inline constexpr bool has_static_draw_v = impl::has_static_draw<T>::value;

} // namespace tt

class IDrawable {
public:
	IDrawable() = default;

	IDrawable(std::string_view name) : hash{ Hash(name) } {}

	using DrawFunc = void (*)(impl::RenderData& rd, const Entity& entity);

	static auto& data() {
		static std::unordered_map<std::size_t, DrawFunc> s;
		return s;
	}

	template <typename T>
	class Registrar {
		friend Entity;

		static constexpr std::string_view GetName() {
			return type_name<T>();
		}

		friend T;

		static bool registerT() {
			static_assert(
				tt::has_static_draw_v<T>,
				"Cannot register draw interface class without static void "
				"Draw(impl::RenderData&, const Entity&) function"
			);
			constexpr auto name{ GetName() };
			std::cout << "Registering draw hash for " << name << std::endl;
			IDrawable::data()[Hash(name)] = &T::Draw;
			return true;
		}

		static bool registered;

		Registrar() {
			(void)registered;
		}
	};

	std::size_t hash{ 0 };
};

template <typename T>
using Drawable = IDrawable::Registrar<T>;

template <typename T>
bool IDrawable::Registrar<T>::registered = IDrawable::Registrar<T>::registerT();

namespace tt {

namespace impl {

template <typename T>
struct is_drawable : std::false_type {};

template <typename T>
struct is_drawable<IDrawable::Registrar<T>> : std::true_type {};

} // namespace impl

template <typename T>
inline constexpr bool is_drawable_v = impl::is_drawable<T>::value;

} // namespace tt

namespace impl {

class RenderData;

} // namespace impl

class Manager;

class Entity : private ecs::Entity {
public:
	using ecs::Entity::Entity;

	Entity()							 = default;
	Entity(const Entity&)				 = default;
	Entity& operator=(const Entity&)	 = default;
	Entity(Entity&&) noexcept			 = default;
	Entity& operator=(Entity&&) noexcept = default;
	~Entity()							 = default;

	friend bool operator==(const Entity& a, const Entity& b) {
		return ecs::Entity{ a } == ecs::Entity{ b };
	}

	friend bool operator!=(const Entity& a, const Entity& b) {
		return !(a == b);
	}

	// Copying a destroyed entity will return Entity{}.
	// Copying an entity with no components simply returns a new entity.
	// Make sure to call manager.Refresh() after this function.
	template <typename... Ts>
	[[nodiscard]] Entity Copy() {
		return ecs::Entity::Copy<Ts...>();
	}

	template <typename T, typename... Ts>
	T& Add(Ts&&... constructor_args) {
		return ecs::Entity::Add<T, Ts...>(std::forward<Ts>(constructor_args)...);
	}

	template <typename... Ts>
	void Remove() {
		ecs::Entity::Remove<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool Has() const {
		return ecs::Entity::Has<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] bool HasAny() const {
		return ecs::Entity::HasAny<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() const {
		return ecs::Entity::Get<Ts...>();
	}

	template <typename... Ts>
	[[nodiscard]] decltype(auto) Get() {
		return ecs::Entity::Get<Ts...>();
	}

	void Clear() const;

	[[nodiscard]] bool IsAlive() const;

	void Destroy();

	[[nodiscard]] Manager& GetManager();

	[[nodiscard]] const Manager& GetManager() const;

	[[nodiscard]] bool IsIdenticalTo(const Entity& e) const;

	// Component manipulation functions.

	template <typename T, tt::enable<tt::has_static_draw_v<T>> = true>
	Entity& SetDraw() {
		Add<IDrawable>(T::GetName());
		return *this;
	}

	[[nodiscard]] bool HasDraw() const;

	Entity& RemoveDraw();

	// @return *this.
	Entity& SetVisible(bool visible);

	// @return *this.
	Entity& Show();

	// @return *this.
	Entity& Hide();

	// @return *this.
	Entity& SetEnabled(bool enabled);

	// @return *this.
	Entity& Disable();

	// @return *this.
	Entity& Enable();

	[[nodiscard]] bool IsVisible() const;
	[[nodiscard]] bool IsEnabled() const;

	[[nodiscard]] UUID GetUUID() const;

	// @return Reference to the transform of the top most parent entity. If none of the entities
	// have a transform component, an assertion is called.
	[[nodiscard]] const Transform& GetRootTransform() const;
	[[nodiscard]] Transform& GetRootTransform();

	[[nodiscard]] Transform GetRelativeTransform() const;

	[[nodiscard]] Transform GetTransform() const;

	[[nodiscard]] V2_float GetRelativePosition() const;

	[[nodiscard]] V2_float GetPosition() const;

	[[nodiscard]] Transform GetRelativeOffset() const;

	[[nodiscard]] Transform GetOffset() const;

	// @return Rotation of the object in radians relative to { 1, 0 }, clockwise positive.
	[[nodiscard]] float GetRelativeRotation() const;

	// @return Rotation of the object in radians relative to its parent object and { 1, 0 },
	// clockwise positive.
	[[nodiscard]] float GetRotation() const;

	[[nodiscard]] V2_float GetRelativeScale() const;

	[[nodiscard]] V2_float GetScale() const;

	// Set the local position of this game object.
	// @return *this.
	Entity& SetPosition(const V2_float& position);

	// Set the local rotation of this game object.
	// @return *this.
	Entity& SetRotation(float rotation);

	// Set the local scale of this game object.
	// @return *this.
	Entity& SetScale(const V2_float& scale);

	Entity& SetDepth(const Depth& depth);

	[[nodiscard]] Depth GetDepth() const;

	Entity& SetBlendMode(BlendMode blend_mode);

	[[nodiscard]] BlendMode GetBlendMode() const;

	Entity& SetOrigin(Origin origin);

	[[nodiscard]] Origin GetOrigin() const;

	Entity& SetTint(const Color& color);

	[[nodiscard]] Color GetTint() const;

	[[nodiscard]] bool IsImmovable() const;

	void AddChild(const Entity& child);

	void AddChild(std::string_view name, const Entity& child);

	// @return Child entity with the given name, or null entity is no such child exists.
	Entity GetChild(std::string_view name);

	void RemoveChild(const Entity& child);
	void RemoveChild(std::string_view name);

	void SetParent(const Entity& child);

	// If object has no parent, returns *this.
	[[nodiscard]] Entity GetParent() const;

	[[nodiscard]] bool HasParent() const;

	[[nodiscard]] std::size_t GetHash() const;

	friend void to_json(json& j, const Entity& e);
	friend void from_json(const json& j, Entity& e);

protected:
	friend class impl::RenderData;

	[[nodiscard]] std::array<V2_float, 4> GetTextureCoordinates(bool flip_vertically) const;

private:
	friend class Manager;

	explicit Entity(const ecs::Entity& e) : ecs::Entity{ e } {}
};

template <typename TCallback, typename... TArgs>
static void Invoke(const Entity& e, TArgs&&... args) {
	if (e.Has<TCallback>()) {
		const auto& callback{ e.Get<TCallback>() };
		Invoke(callback, std::forward<TArgs>(args)...);
	}
}

} // namespace ptgn

namespace std {

template <>
struct hash<ptgn::Entity> {
	size_t operator()(const ptgn::Entity& entity) const {
		return entity.GetHash();
	}
};

} // namespace std

namespace ptgn {

struct Children {
	Children(std::string_view name, const Entity& child);
	Children(const Entity& child);

	void Add(const Entity& child);
	void Remove(const Entity& child);

	void Add(std::string_view name, const Entity& child);
	void Remove(std::string_view name);
	// @return Entity with given name, or null entity is no such entity exists.
	Entity Get(std::string_view name);

	[[nodiscard]] bool IsEmpty() const;

private:
	std::unordered_set<Entity> children_;
	std::unordered_map<std::size_t, Entity> named_children_;
};

} // namespace ptgn