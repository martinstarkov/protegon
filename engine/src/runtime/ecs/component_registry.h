#pragma once

#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include <magic_enum/magic_enum.hpp>

#include "core/util/hash.h"
#include "core/util/reflection.h"
#include "core/util/type_info.h"
#include "runtime/ecs/entity.h"
#include "serialization/json/json.h"

namespace ptgn {

enum class ComponentReflectionNodeKind {
	Value,
	BeginObject,
	EndObject,
	BeginOptional,
	EndOptional,
	BeginSequence,
	EndSequence,
	BeginSequenceElement,
	EndSequenceElement,
	BeginVariant,
	EndVariant,
};

enum class ComponentReflectionValueKind {
	Unknown,
	Bool,
	SignedInteger,
	UnsignedInteger,
	FloatingPoint,
	String,
	Enum,
};

struct ReflectedComponentMember;

using ReflectedComponentMemberCallback =
	void (*)(void* user_data, const ReflectedComponentMember& member);
using ReflectedComponentShouldVisitChildrenCallback =
	bool (*)(void* user_data, const ReflectedComponentMember& member);
using ReflectedComponentSetBoolCallback = bool (*)(void* value, bool updated);
using ReflectedComponentSetSignedCallback = bool (*)(void* value, std::int64_t updated);
using ReflectedComponentSetUnsignedCallback = bool (*)(void* value, std::uint64_t updated);
using ReflectedComponentSetFloatCallback = bool (*)(void* value, double updated);
using ReflectedComponentOptionalSetCallback = bool (*)(void* value, bool enabled);
using ReflectedComponentSequenceInsertCallback = bool (*)(void* value, std::size_t index);
using ReflectedComponentSequenceEraseCallback = bool (*)(void* value, std::size_t index);
using ReflectedComponentSequenceMoveCallback =
	bool (*)(void* value, std::size_t from, std::size_t to);
using ReflectedComponentVariantSetCallback = bool (*)(void* value, std::size_t index);
using ReflectedComponentEnumSetCallback = bool (*)(void* value, std::size_t index);
using ReflectedComponentIndexedNameCallback = std::string_view (*)(std::size_t index);

struct ReflectedComponentMember {
	ComponentReflectionNodeKind kind{ ComponentReflectionNodeKind::Value };
	ComponentReflectionValueKind value_kind{ ComponentReflectionValueKind::Unknown };
	std::string name{};
	std::size_t type_id{ 0 };
	const void* value{ nullptr };
	void* mutable_value{ nullptr };
	bool read_only{ false };
	std::size_t depth{ 0 };

	bool bool_value{ false };
	std::int64_t signed_value{ 0 };
	std::uint64_t unsigned_value{ 0 };
	double floating_value{ 0.0 };

	ReflectedComponentSetBoolCallback set_bool{ nullptr };
	ReflectedComponentSetSignedCallback set_signed{ nullptr };
	ReflectedComponentSetUnsignedCallback set_unsigned{ nullptr };
	ReflectedComponentSetFloatCallback set_float{ nullptr };

	bool optional_has_value{ false };
	ReflectedComponentOptionalSetCallback optional_set{ nullptr };

	std::size_t sequence_size{ 0 };
	std::size_t sequence_index{ 0 };
	bool sequence_resizable{ false };
	ReflectedComponentSequenceInsertCallback sequence_insert{ nullptr };
	ReflectedComponentSequenceEraseCallback sequence_erase{ nullptr };
	ReflectedComponentSequenceMoveCallback sequence_move{ nullptr };

	std::size_t variant_index{ 0 };
	std::size_t variant_count{ 0 };
	ReflectedComponentIndexedNameCallback variant_name{ nullptr };
	ReflectedComponentVariantSetCallback variant_set{ nullptr };

	std::size_t enum_count{ 0 };
	std::size_t enum_index{ 0 };
	ReflectedComponentIndexedNameCallback enum_name{ nullptr };
	ReflectedComponentEnumSetCallback enum_set{ nullptr };
};

struct ComponentReflectionVisitor {
	void* user_data{ nullptr };
	ReflectedComponentMemberCallback callback{ nullptr };
	ReflectedComponentShouldVisitChildrenCallback should_visit_children{ nullptr };
};

enum class ComponentOperation {
	Has,
	Remove,
	AddDefault,
	Serialize,
	Deserialize,
	MakeDefaultJson,
	VisitValue,
};

struct ComponentOperationContext {
	ComponentOperation operation{ ComponentOperation::Has };
	Entity entity{};
	json* output{ nullptr };
	const json* input{ nullptr };
	void* value{ nullptr };
	ComponentReflectionVisitor reflection{};
	bool supported{ false };
	bool success{ false };
	bool bool_result{ false };
};

using ComponentDispatchCallback = void (*)(ComponentOperationContext& context);

namespace impl {

template <typename T>
concept JsonGettable = requires(const json& input) {
	{ input.template get<T>() } -> std::same_as<T>;
};

template <typename T>
concept HasReflectedMembers = requires(T& value) { ReflectMembers(value); };

template <typename T>
concept HasReflectedReadOnlyMembers = requires(const T& value) {
	ReflectReadOnlyMembers(value);
};

template <typename T>
concept HasReflectedValue = requires(T& value) { ReflectValue(value); };

template <typename T>
struct IsOptional : std::false_type {};

template <typename T>
struct IsOptional<std::optional<T>> : std::true_type {};

template <typename T>
inline constexpr bool kIsOptional{ IsOptional<std::remove_cvref_t<T>>::value };

template <typename T>
struct IsVector : std::false_type {};

template <typename T, typename Allocator>
struct IsVector<std::vector<T, Allocator>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVector{ IsVector<std::remove_cvref_t<T>>::value };

template <typename T>
struct IsArray : std::false_type {};

template <typename T, std::size_t N>
struct IsArray<std::array<T, N>> : std::true_type {};

template <typename T>
inline constexpr bool kIsArray{ IsArray<std::remove_cvref_t<T>>::value };

template <typename T>
struct IsVariant : std::false_type {};

template <typename... T>
struct IsVariant<std::variant<T...>> : std::true_type {};

template <typename T>
inline constexpr bool kIsVariant{ IsVariant<std::remove_cvref_t<T>>::value };

template <typename T>
inline constexpr bool kIsMagicEnumReflected{
	magic_enum::detail::is_reflected_v<
		std::remove_cvref_t<T>,
		magic_enum::detail::enum_subtype::common
	>
};

template <typename T>
[[nodiscard]] void* MutableReflectionPointer(T& value, bool read_only) {
	if (read_only) {
		return nullptr;
	}

	if constexpr (std::is_const_v<std::remove_reference_t<T>>) {
		return nullptr;
	} else {
		return std::addressof(value);
	}
}

template <typename T>
bool SetReflectionBool(void* value, bool updated) {
	if (!value) {
		return false;
	}

	auto& typed{ *static_cast<T*>(value) };
	const T converted{ static_cast<T>(updated) };
	if (typed == converted) {
		return false;
	}
	typed = converted;
	return true;
}

template <typename T>
bool SetReflectionSigned(void* value, std::int64_t updated) {
	if (!value) {
		return false;
	}

	auto& typed{ *static_cast<T*>(value) };
	const T converted{ static_cast<T>(updated) };
	if (typed == converted) {
		return false;
	}
	typed = converted;
	return true;
}

template <typename T>
bool SetReflectionUnsigned(void* value, std::uint64_t updated) {
	if (!value) {
		return false;
	}

	auto& typed{ *static_cast<T*>(value) };
	const T converted{ static_cast<T>(updated) };
	if (typed == converted) {
		return false;
	}
	typed = converted;
	return true;
}

template <typename T>
bool SetReflectionFloat(void* value, double updated) {
	if (!value) {
		return false;
	}

	auto& typed{ *static_cast<T*>(value) };
	const T converted{ static_cast<T>(updated) };
	if (typed == converted) {
		return false;
	}
	typed = converted;
	return true;
}

template <typename Optional>
bool SetReflectionOptional(void* value, bool enabled) {
	if (!value) {
		return false;
	}

	auto& optional{ *static_cast<Optional*>(value) };
	if (enabled == optional.has_value()) {
		return false;
	}

	if (!enabled) {
		optional.reset();
		return true;
	}

	using Element = typename Optional::value_type;
	if constexpr (std::default_initializable<Element>) {
		optional.emplace();
		return true;
	}

	return false;
}

template <typename Vector>
bool InsertReflectionSequence(void* value, std::size_t index) {
	if (!value) {
		return false;
	}

	using Element = typename Vector::value_type;
	if constexpr (!std::default_initializable<Element>) {
		return false;
	} else {
		auto& vector{ *static_cast<Vector*>(value) };
		index = std::min(index, vector.size());
		vector.emplace(vector.begin() + static_cast<std::ptrdiff_t>(index));
		return true;
	}
}

template <typename Vector>
bool EraseReflectionSequence(void* value, std::size_t index) {
	if (!value) {
		return false;
	}

	auto& vector{ *static_cast<Vector*>(value) };
	if (index >= vector.size()) {
		return false;
	}
	vector.erase(vector.begin() + static_cast<std::ptrdiff_t>(index));
	return true;
}

template <typename Vector>
bool MoveReflectionSequence(void* value, std::size_t from, std::size_t to) {
	if (!value) {
		return false;
	}

	auto& vector{ *static_cast<Vector*>(value) };
	if (from >= vector.size() || to >= vector.size() || from == to) {
		return false;
	}
	std::ranges::iter_swap(
		vector.begin() + static_cast<std::ptrdiff_t>(from),
		vector.begin() + static_cast<std::ptrdiff_t>(to)
	);
	return true;
}

template <typename Variant, std::size_t I = 0>
bool SetReflectionVariantIndex(Variant& value, std::size_t index) {
	if constexpr (I >= std::variant_size_v<Variant>) {
		return false;
	} else {
		if (index == I) {
			using Alternative = std::variant_alternative_t<I, Variant>;
			if constexpr (std::default_initializable<Alternative>) {
				if (value.index() != I) {
					value.template emplace<I>();
					return true;
				}
			}
			return false;
		}
		return SetReflectionVariantIndex<Variant, I + 1>(value, index);
	}
}

template <typename Variant>
bool SetReflectionVariant(void* value, std::size_t index) {
	if (!value) {
		return false;
	}
	return SetReflectionVariantIndex(*static_cast<Variant*>(value), index);
}

template <typename Variant, std::size_t I = 0>
std::string_view ReflectionVariantName(std::size_t index) {
	if constexpr (I >= std::variant_size_v<Variant>) {
		return {};
	} else {
		if (index == I) {
			return type_name_without_namespaces<std::variant_alternative_t<I, Variant>>();
		}
		return ReflectionVariantName<Variant, I + 1>(index);
	}
}

template <typename Enum>
bool SetReflectionEnumIndex(void* value, std::size_t index) {
	if (!value) {
		return false;
	}

	constexpr auto values{ magic_enum::enum_values<Enum>() };
	if (index >= values.size()) {
		return false;
	}

	auto& typed{ *static_cast<Enum*>(value) };
	if (typed == values[index]) {
		return false;
	}
	typed = values[index];
	return true;
}

template <typename Enum>
std::string_view ReflectionEnumName(std::size_t index) {
	constexpr auto names{ magic_enum::enum_names<Enum>() };
	return index < names.size() ? names[index] : std::string_view{};
}

template <typename T>
ReflectedComponentMember MakeReflectionNode(
	ComponentReflectionNodeKind kind,
	std::string_view name,
	T& value,
	bool read_only,
	std::size_t depth
) {
	using Value = std::remove_cvref_t<T>;
	ReflectedComponentMember result{
		.kind = kind,
		.name = std::string{ name },
		.type_id = Hash<Value>(),
		.value = std::addressof(value),
		.mutable_value = MutableReflectionPointer(value, read_only),
		.read_only = read_only,
		.depth = depth,
	};

	if constexpr (std::same_as<Value, bool>) {
		result.value_kind = ComponentReflectionValueKind::Bool;
		result.bool_value = value;
		result.set_bool = &SetReflectionBool<Value>;
	} else if constexpr (std::is_enum_v<Value>) {
		using Underlying = std::underlying_type_t<Value>;

		if constexpr (kIsMagicEnumReflected<Value>) {
			result.value_kind = ComponentReflectionValueKind::Enum;
			result.enum_count = magic_enum::enum_count<Value>();
			result.enum_index = magic_enum::enum_index(value).value_or(0);
			result.enum_name = &ReflectionEnumName<Value>;
			result.enum_set = &SetReflectionEnumIndex<Value>;
		} else if constexpr (std::is_signed_v<Underlying>) {
			result.value_kind = ComponentReflectionValueKind::SignedInteger;
			result.signed_value = static_cast<std::int64_t>(
				static_cast<Underlying>(value)
			);
			result.set_signed = &SetReflectionSigned<Value>;
		} else {
			result.value_kind = ComponentReflectionValueKind::UnsignedInteger;
			result.unsigned_value = static_cast<std::uint64_t>(
				static_cast<Underlying>(value)
			);
			result.set_unsigned = &SetReflectionUnsigned<Value>;
		}
	} else if constexpr (std::integral<Value> && std::is_signed_v<Value>) {
		result.value_kind = ComponentReflectionValueKind::SignedInteger;
		result.signed_value = static_cast<std::int64_t>(value);
		result.set_signed = &SetReflectionSigned<Value>;
	} else if constexpr (std::integral<Value> && std::is_unsigned_v<Value>) {
		result.value_kind = ComponentReflectionValueKind::UnsignedInteger;
		result.unsigned_value = static_cast<std::uint64_t>(value);
		result.set_unsigned = &SetReflectionUnsigned<Value>;
	} else if constexpr (std::floating_point<Value>) {
		result.value_kind = ComponentReflectionValueKind::FloatingPoint;
		result.floating_value = static_cast<double>(value);
		result.set_float = &SetReflectionFloat<Value>;
	} else if constexpr (std::same_as<Value, std::string>) {
		result.value_kind = ComponentReflectionValueKind::String;
	}

	return result;
}

inline void EmitReflectionNode(
	const ComponentReflectionVisitor& visitor,
	const ReflectedComponentMember& member
) {
	if (visitor.callback) {
		visitor.callback(visitor.user_data, member);
	}
}

inline bool ShouldVisitReflectionChildren(
	const ComponentReflectionVisitor& visitor,
	const ReflectedComponentMember& member
) {
	return !visitor.should_visit_children ||
		visitor.should_visit_children(visitor.user_data, member);
}

template <typename T>
void VisitReflectedValue(
	std::string_view name,
	T& value,
	bool read_only,
	std::size_t depth,
	ComponentReflectionVisitor visitor
);

template <typename T>
void VisitReflectedObject(
	std::string_view name,
	T& value,
	bool read_only,
	std::size_t depth,
	ComponentReflectionVisitor visitor
) {
	using Value = std::remove_cvref_t<T>;
	auto begin{ MakeReflectionNode(
		ComponentReflectionNodeKind::BeginObject, name, value, read_only, depth
	) };
	EmitReflectionNode(visitor, begin);

	if (ShouldVisitReflectionChildren(visitor, begin)) {
		if constexpr (HasReflectedValue<Value>) {
			auto member{ ReflectValue(value) };
			VisitReflectedValue(member.name, member.value, read_only, depth + 1, visitor);
		} else {
			if constexpr (HasReflectedMembers<Value>) {
				auto members{ ReflectMembers(value) };
				std::apply(
					[&](auto&&... member) {
						(VisitReflectedValue(
							member.name, member.value, read_only, depth + 1, visitor
						), ...);
					},
					members
				);
			}

			if constexpr (HasReflectedReadOnlyMembers<Value>) {
				const Value& const_value{ value };
				auto members{ ReflectReadOnlyMembers(const_value) };
				std::apply(
					[&](auto&&... member) {
						(VisitReflectedValue(
							member.name, member.value, true, depth + 1, visitor
						), ...);
					},
					members
				);
			}
		}
	}

	EmitReflectionNode(
		visitor,
		MakeReflectionNode(ComponentReflectionNodeKind::EndObject, name, value, read_only, depth)
	);
}

template <typename T>
void VisitReflectedValue(
	std::string_view name,
	T& value,
	bool read_only,
	std::size_t depth,
	ComponentReflectionVisitor visitor
) {
	using Value = std::remove_cvref_t<T>;

	if constexpr (kIsOptional<Value>) {
		auto begin{ MakeReflectionNode(
			ComponentReflectionNodeKind::BeginOptional, name, value, read_only, depth
		) };
		begin.optional_has_value = value.has_value();
		begin.optional_set = &SetReflectionOptional<Value>;
		EmitReflectionNode(visitor, begin);
		if (ShouldVisitReflectionChildren(visitor, begin) && value.has_value()) {
			VisitReflectedValue("Value", *value, read_only, depth + 1, visitor);
		}
		EmitReflectionNode(
			visitor,
			MakeReflectionNode(ComponentReflectionNodeKind::EndOptional, name, value, read_only, depth)
		);
	} else if constexpr (kIsVector<Value>) {
		auto begin{ MakeReflectionNode(
			ComponentReflectionNodeKind::BeginSequence, name, value, read_only, depth
		) };
		begin.sequence_size = value.size();
		begin.sequence_resizable = true;
		begin.sequence_insert = &InsertReflectionSequence<Value>;
		begin.sequence_erase = &EraseReflectionSequence<Value>;
		begin.sequence_move = &MoveReflectionSequence<Value>;
		EmitReflectionNode(visitor, begin);

		if (ShouldVisitReflectionChildren(visitor, begin)) {
			if constexpr (std::same_as<typename Value::value_type, bool>) {
				EmitReflectionNode(
					visitor,
					MakeReflectionNode(ComponentReflectionNodeKind::Value, name, value, read_only, depth + 1)
				);
			} else {
				const std::size_t count{ value.size() };
				for (std::size_t index{ 0 }; index < count; ++index) {
					auto element{ MakeReflectionNode(
						ComponentReflectionNodeKind::BeginSequenceElement,
						std::to_string(index), value, read_only, depth + 1
					) };
					element.sequence_index = index;
					element.sequence_size = count;
					element.sequence_resizable = true;
					element.sequence_erase = begin.sequence_erase;
					element.sequence_move = begin.sequence_move;
					EmitReflectionNode(visitor, element);
					if (ShouldVisitReflectionChildren(visitor, element)) {
						VisitReflectedValue("Value", value[index], read_only, depth + 2, visitor);
					}
					EmitReflectionNode(
						visitor,
						MakeReflectionNode(
							ComponentReflectionNodeKind::EndSequenceElement,
							std::to_string(index), value, read_only, depth + 1
						)
					);
				}
			}
		}

		auto end{ MakeReflectionNode(
			ComponentReflectionNodeKind::EndSequence, name, value, read_only, depth
		) };
		end.sequence_size = value.size();
		end.sequence_resizable = true;
		end.sequence_insert = begin.sequence_insert;
		end.sequence_erase = begin.sequence_erase;
		end.sequence_move = begin.sequence_move;
		EmitReflectionNode(visitor, end);
	} else if constexpr (kIsArray<Value>) {
		auto begin{ MakeReflectionNode(
			ComponentReflectionNodeKind::BeginSequence, name, value, read_only, depth
		) };
		begin.sequence_size = value.size();
		EmitReflectionNode(visitor, begin);
		if (ShouldVisitReflectionChildren(visitor, begin)) {
			for (std::size_t index{ 0 }; index < value.size(); ++index) {
				VisitReflectedValue(std::to_string(index), value[index], read_only, depth + 1, visitor);
			}
		}
		auto end{ MakeReflectionNode(
			ComponentReflectionNodeKind::EndSequence, name, value, read_only, depth
		) };
		end.sequence_size = value.size();
		EmitReflectionNode(visitor, end);
	} else if constexpr (kIsVariant<Value>) {
		auto begin{ MakeReflectionNode(
			ComponentReflectionNodeKind::BeginVariant, name, value, read_only, depth
		) };
		begin.variant_index = value.index();
		begin.variant_count = std::variant_size_v<Value>;
		begin.variant_name = &ReflectionVariantName<Value>;
		begin.variant_set = &SetReflectionVariant<Value>;
		EmitReflectionNode(visitor, begin);
		if (ShouldVisitReflectionChildren(visitor, begin)) {
			std::visit(
				[&](auto& active) {
					VisitReflectedValue("Value", active, read_only, depth + 1, visitor);
				},
				value
			);
		}
		EmitReflectionNode(
			visitor,
			MakeReflectionNode(ComponentReflectionNodeKind::EndVariant, name, value, read_only, depth)
		);
	} else if constexpr (
		HasReflectedValue<Value> || HasReflectedMembers<Value> || HasReflectedReadOnlyMembers<Value>
	) {
		VisitReflectedObject(name, value, read_only, depth, visitor);
	} else {
		EmitReflectionNode(
			visitor,
			MakeReflectionNode(ComponentReflectionNodeKind::Value, name, value, read_only, depth)
		);
	}
}

template <typename T>
void DispatchRegisteredComponent(ComponentOperationContext& context) {
	switch (context.operation) {
		case ComponentOperation::Has:
			context.supported = true;
			context.success = true;
			context.bool_result = context.entity.Has<T>();
			break;
		case ComponentOperation::Remove:
			context.supported = true;
			if (context.entity.Has<T>()) {
				context.entity.Remove<T>();
			}
			context.success = true;
			break;
		case ComponentOperation::AddDefault:
			if constexpr (std::default_initializable<T>) {
				context.supported = true;
				if (!context.entity.Has<T>()) {
					context.entity.Add<T>();
				}
				context.success = true;
			}
			break;
		case ComponentOperation::Serialize:
			if constexpr (JsonSerializable<T>) {
				context.supported = true;
				if (context.output && context.entity.Has<T>()) {
					*context.output = context.entity.Get<T>();
					context.success = true;
				}
			}
			break;
		case ComponentOperation::Deserialize:
			if constexpr (
				JsonDeserializable<T> && (std::default_initializable<T> || JsonGettable<T>)
			) {
				context.supported = true;
				if (!context.input) {
					break;
				}
				if (context.entity.Has<T>()) {
					context.input->get_to(context.entity.Get<T>());
				} else if constexpr (std::default_initializable<T>) {
					context.entity.Add<T>();
					context.input->get_to(context.entity.Get<T>());
				} else {
					context.entity.Add<T>(context.input->template get<T>());
				}
				context.success = true;
			}
			break;
		case ComponentOperation::MakeDefaultJson:
			if constexpr (std::default_initializable<T> && JsonSerializable<T>) {
				context.supported = true;
				if (context.output) {
					*context.output = T{};
					context.success = true;
				}
			}
			break;
		case ComponentOperation::VisitValue:
			context.supported = true;
			if (context.value && context.reflection.callback) {
				VisitReflectedValue(
					"", *static_cast<T*>(context.value), false, 0, context.reflection
				);
				context.success = true;
			}
			break;
	}
}

void EnsureEngineComponentsRegistered();

} // namespace impl

template <typename T>
void VisitReflectedValue(T& value, ComponentReflectionVisitor visitor) {
	impl::VisitReflectedValue("", value, false, 0, visitor);
}

struct RegisteredComponent {
	std::size_t type_id{ 0 };
	std::string name{};
	bool is_empty{ false };
	bool default_constructible{ false };
	bool serializable{ false };
	bool deserializable{ false };
	bool reflectable{ false };
	ComponentDispatchCallback dispatch{ nullptr };

	[[nodiscard]] bool Has(Entity entity) const {
		ComponentOperationContext context{ .operation = ComponentOperation::Has, .entity = entity };
		dispatch(context);
		return context.bool_result;
	}

	bool Remove(Entity entity) const {
		ComponentOperationContext context{ .operation = ComponentOperation::Remove, .entity = entity };
		dispatch(context);
		return context.success;
	}

	bool AddDefault(Entity entity) const {
		ComponentOperationContext context{ .operation = ComponentOperation::AddDefault, .entity = entity };
		dispatch(context);
		return context.success;
	}

	bool Serialize(json& output, Entity entity) const {
		ComponentOperationContext context{
			.operation = ComponentOperation::Serialize,
			.entity = entity,
			.output = std::addressof(output),
		};
		dispatch(context);
		return context.success;
	}

	bool Deserialize(const json& input, Entity entity) const {
		ComponentOperationContext context{
			.operation = ComponentOperation::Deserialize,
			.entity = entity,
			.input = std::addressof(input),
		};
		dispatch(context);
		return context.success;
	}

	[[nodiscard]] std::optional<json> MakeDefaultJson() const {
		json output;
		ComponentOperationContext context{
			.operation = ComponentOperation::MakeDefaultJson,
			.output = std::addressof(output),
		};
		dispatch(context);
		return context.success ? std::optional<json>{ std::move(output) } : std::nullopt;
	}

	bool Visit(void* value, ComponentReflectionVisitor visitor) const {
		ComponentOperationContext context{
			.operation = ComponentOperation::VisitValue,
			.value = value,
			.reflection = visitor,
		};
		dispatch(context);
		return context.success;
	}
};

class ComponentRegistry {
public:
	template <typename T>
	static bool Register(std::string_view name) {
		using Component = std::remove_cvref_t<T>;
		auto& components{ MutableComponents() };
		const std::size_t type_id{ Hash<Component>() };

		for (auto& component : components) {
			if (component.type_id == type_id) {
				component.name = std::string{ name };
				return false;
			}
			if (component.name == name) {
				return false;
			}
		}

		constexpr bool deserializable{
			JsonDeserializable<Component> &&
			(std::default_initializable<Component> || impl::JsonGettable<Component>)
		};
		constexpr bool reflectable{
			impl::HasReflectedValue<Component> || impl::HasReflectedMembers<Component> ||
			impl::HasReflectedReadOnlyMembers<Component> || impl::kIsOptional<Component> ||
			impl::kIsVector<Component> || impl::kIsArray<Component> || impl::kIsVariant<Component> ||
			std::is_arithmetic_v<Component> || std::is_enum_v<Component> ||
			std::same_as<Component, std::string>
		};

		components.push_back(RegisteredComponent{
			.type_id = type_id,
			.name = std::string{ name },
			.is_empty = std::is_empty_v<Component>,
			.default_constructible = std::default_initializable<Component>,
			.serializable = JsonSerializable<Component>,
			.deserializable = deserializable,
			.reflectable = reflectable,
			.dispatch = &impl::DispatchRegisteredComponent<Component>,
		});
		return true;
	}

	[[nodiscard]] static const std::vector<RegisteredComponent>& Components() {
		impl::EnsureEngineComponentsRegistered();
		return MutableComponents();
	}

	[[nodiscard]] static const RegisteredComponent* Find(std::string_view name) {
		for (const auto& component : Components()) {
			if (component.name == name) {
				return std::addressof(component);
			}
		}
		return nullptr;
	}

	[[nodiscard]] static const RegisteredComponent* Find(std::size_t type_id) {
		for (const auto& component : Components()) {
			if (component.type_id == type_id) {
				return std::addressof(component);
			}
		}
		return nullptr;
	}

	template <typename T>
	[[nodiscard]] static const RegisteredComponent* Find() {
		return Find(Hash<std::remove_cvref_t<T>>());
	}

private:
	[[nodiscard]] static std::vector<RegisteredComponent>& MutableComponents() {
		static std::vector<RegisteredComponent> components;
		return components;
	}
};

} // namespace ptgn
