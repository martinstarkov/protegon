#pragma once

#include <concepts>
#include <memory>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "core/util/reflection.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"

namespace ptgn {

namespace impl {

template <typename T>
[[nodiscard]] bool HasRegisteredComponent(Entity entity) {
	return entity.Has<T>();
}

template <typename T>
void RemoveRegisteredComponent(Entity entity) {
	entity.Remove<T>();
}

template <typename T>
void AddDefaultRegisteredComponent(Entity entity) {
	entity.Add<T>();
}

template <typename T>
void SerializeRegisteredComponent(json& output, Entity entity) {
	output = entity.Get<T>();
}

template <typename T>
[[nodiscard]] json MakeDefaultRegisteredComponentJson() {
	json output = T{};
	return output;
}

template <typename T>
concept JsonGettable = requires(const json& input) {
	{ input.template get<T>() } -> std::same_as<T>;
};

template <typename T>
void DeserializeRegisteredComponent(const json& input, Entity entity) {
	if (entity.Has<T>()) {
		input.get_to(entity.Get<T>());
		return;
	}

	if constexpr (std::default_initializable<T>) {
		entity.Add<T>();
		input.get_to(entity.Get<T>());
	} else {
		static_assert(
			JsonGettable<T>,
			"A registered non default constructible component must support json::get<T>()"
		);
		entity.Add<T>(input.template get<T>());
	}
}

} // namespace impl

struct ReflectedComponentMember {
	std::string name;
	std::size_t type_id{ 0 };
	const void* value{ nullptr };
	void* mutable_value{ nullptr };
	bool read_only{ false };
};

using ReflectedComponentMemberCallback =
	void (*)(void* user_data, const ReflectedComponentMember& member);

struct ComponentReflectionVisitor {
	void* user_data{ nullptr };
	ReflectedComponentMemberCallback callback{ nullptr };
};

namespace impl {

template <typename T>
concept HasReflectedMembers = requires(T& value) { ReflectMembers(value); };

template <typename T>
concept HasReflectedReadOnlyMembers = requires(const T& value) { ReflectReadOnlyMembers(value); };

template <typename T>
void VisitRegisteredComponentMembers(Entity entity, ComponentReflectionVisitor visitor) {
	if (!visitor.callback) {
		return;
	}

	auto& component{ entity.Get<T>() };

	if constexpr (HasReflectedMembers<T>) {
		auto members{ ReflectMembers(component) };

		std::apply(
			[&]<typename... TMember>(TMember&&... member) {
				(visitor.callback(
					 visitor.user_data,
					 ReflectedComponentMember{
						 .name			= std::string{ member.name },
						 .type_id		= Hash<decltype(member.value)>(),
						 .value			= std::addressof(member.value),
						 .mutable_value = std::addressof(member.value),
						 .read_only		= false,
					 }
				 ),
				 ...);
			},
			members
		);
	}

	if constexpr (HasReflectedReadOnlyMembers<T>) {
		auto members{ ReflectReadOnlyMembers(component) };

		std::apply(
			[&]<typename... TMember>(TMember&&... member) {
				(visitor.callback(
					 visitor.user_data,
					 ReflectedComponentMember{
						 .name			= std::string{ member.name },
						 .type_id		= Hash<decltype(member.value)>(),
						 .value			= std::addressof(member.value),
						 .mutable_value = nullptr,
						 .read_only		= true,
					 }
				 ),
				 ...);
			},
			members
		);
	}
}

/// @brief Ensures the translation unit containing the engine component
/// registrations is linked into the application.
void EnsureEngineComponentsRegistered();

} // namespace impl

using ComponentHasCallback			   = bool (*)(Entity entity);
using ComponentRemoveCallback		   = void (*)(Entity entity);
using ComponentAddDefaultCallback	   = void (*)(Entity entity);
using ComponentSerializeCallback	   = void (*)(json& output, Entity entity);
using ComponentDeserializeCallback	   = void (*)(const json& input, Entity entity);
using ComponentMakeDefaultJsonCallback = json (*)();
using ComponentVisitMembersCallback = void (*)(Entity entity, ComponentReflectionVisitor visitor);

struct RegisteredComponent {
	std::size_t type_id{ 0 };
	std::string name;
	bool is_empty{ false };
	bool default_constructible{ false };
	bool serializable{ false };
	bool deserializable{ false };

	ComponentHasCallback has{ nullptr };
	ComponentRemoveCallback remove{ nullptr };
	ComponentAddDefaultCallback add_default{ nullptr };
	ComponentSerializeCallback serialize{ nullptr };
	ComponentDeserializeCallback deserialize{ nullptr };
	ComponentMakeDefaultJsonCallback make_default_json{ nullptr };
	ComponentVisitMembersCallback visit_members{ nullptr };
};

class ComponentRegistry {
public:
	template <typename T>
	static bool Register(std::string_view name) {
		using Component = std::remove_cvref_t<T>;

		auto& components{ MutableComponents() };
		auto type_id{ Hash<Component>() };

		for (auto& component : components) {
			if (component.type_id == type_id) {
				component.name = std::string{ name };
				return false;
			}

			if (component.name == name) {
				return false;
			}
		}

		ComponentAddDefaultCallback add_default{ nullptr };
		ComponentSerializeCallback serialize{ nullptr };
		ComponentDeserializeCallback deserialize{ nullptr };
		ComponentMakeDefaultJsonCallback make_default_json{ nullptr };
		ComponentVisitMembersCallback visit_members{ nullptr };

		if constexpr (std::default_initializable<Component>) {
			add_default = &impl::AddDefaultRegisteredComponent<Component>;
		}

		if constexpr (JsonSerializable<Component>) {
			serialize = &impl::SerializeRegisteredComponent<Component>;
		}

		if constexpr (std::default_initializable<Component> && JsonSerializable<Component>) {
			make_default_json = &impl::MakeDefaultRegisteredComponentJson<Component>;
		}

		if constexpr (
			impl::HasReflectedMembers<Component> || impl::HasReflectedReadOnlyMembers<Component>
		) {
			visit_members = &impl::VisitRegisteredComponentMembers<Component>;
		}

		if constexpr (
			JsonDeserializable<Component> &&
			(std::default_initializable<Component> || impl::JsonGettable<Component>)
		) {
			deserialize = &impl::DeserializeRegisteredComponent<Component>;
		}

		components.push_back(
			RegisteredComponent{
				.type_id			   = type_id,
				.name				   = std::string{ name },
				.is_empty			   = std::is_empty_v<Component>,
				.default_constructible = std::default_initializable<Component>,
				.serializable		   = serialize != nullptr,
				.deserializable		   = deserialize != nullptr,
				.has				   = &impl::HasRegisteredComponent<Component>,
				.remove				   = &impl::RemoveRegisteredComponent<Component>,
				.add_default		   = add_default,
				.serialize			   = serialize,
				.deserialize		   = deserialize,
				.make_default_json	   = make_default_json,
				.visit_members		   = visit_members,
			}
		);

		return true;
	}

	[[nodiscard]] static const std::vector<RegisteredComponent>& Components() {
		impl::EnsureEngineComponentsRegistered();
		return MutableComponents();
	}

	[[nodiscard]] static const RegisteredComponent* Find(std::string_view name) {
		for (const auto& component : Components()) {
			if (component.name == name) {
				return &component;
			}
		}

		return nullptr;
	}

	[[nodiscard]] static const RegisteredComponent* Find(std::size_t type_id) {
		for (const auto& component : Components()) {
			if (component.type_id == type_id) {
				return &component;
			}
		}

		return nullptr;
	}

	template <typename T>
	[[nodiscard]] static const RegisteredComponent* Find() {
		return Find(Hash<T>());
	}

private:
	[[nodiscard]] static std::vector<RegisteredComponent>& MutableComponents() {
		static std::vector<RegisteredComponent> components;
		return components;
	}
};

} // namespace ptgn
