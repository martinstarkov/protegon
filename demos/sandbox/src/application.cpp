/*
#include "components/movement.h"
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/collision/collider.h"
#include "renderer/color.h"
#include "renderer/texture.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"
#include "serialization/file_stream_reader.h"
#include "serialization/file_stream_writer.h"
#include "serialization/stream_reader.h"
#include "serialization/stream_writer.h"
#include "utility/file.h"
#include "utility/log.h"

using namespace ptgn;

class Sandbox : public Scene {
public:
	struct OnCollision {
		virtual ~OnCollision() = default;

		virtual void Start(Collision c) {}

		virtual void Continue(Collision c) {}

		virtual void Stop(Collision c) {}
	};

	struct CollisionCallback {
		template <typename T>
		CollisionCallback(T&& t) : ptr{ std::make_unique<T>(t) } {}

		std::unique_ptr<OnCollision> ptr;
	};

	struct OnPlayerCollision : public OnCollision {
		OnPlayerCollision(ecs::Entity player) : player{ player } {}

		ecs::Entity player;

		void Start(Collision c) override {
			PTGN_LOG("Start | Player id: ", player.GetId());
		}

		void Continue(Collision c) override {
			PTGN_LOG("Continue | Player id: ", player.GetId());
		}

		void Stop(Collision c) override {
			PTGN_LOG("Stop | Player id: ", player.GetId());
		}

		static void Serialize(StreamWriter* w, const OnPlayerCollision& p) {
			w->Write(p.player.GetId());
		}

		static void Deserialize(StreamReader* r, OnPlayerCollision& p) {
			ecs::Index player_index;
			r->Read(player_index);
			PTGN_LOG("Deserialized player index: ", player_index);
		}
	};

	struct TargetPosition {
		V2_float start;
		V2_float stop;
	};

	ecs::Entity CreateSheep(ecs::Manager& m, const V2_float& position) {
		auto e = manager.CreateEntity();
		e.Add<Transform>(position);
		e.Add<Visible>();
		e.Add<TextureKey>("sheep");
		e.Add<TargetPosition>();
		V2_float ws{ game.window.GetSize() };
		std::array<V2_float, 4> coordinates{
			V2_float{},
			V2_float{ ws.x, 0.0f },
			ws,
			V2_float{ 0.0f, ws.y },
		};
		e.Add<Tween>()
			.During(milliseconds{ 4000 })
			.Repeat(-1)
			.OnRepeat([e, coordinates]() {
				auto index = e.Get<Tween>().GetRepeats() % coordinates.size();
				PTGN_ASSERT(index < coordinates.size());
				e.Get<TargetPosition>().start = e.Get<Transform>().position;
				e.Get<TargetPosition>().stop  = coordinates[index];
			})
			.OnUpdate([e](float f) {
				e.Get<Transform>().position =
					Lerp(e.Get<TargetPosition>().start, e.Get<TargetPosition>().stop, f);
			})
			.Start();
		return e;
	}

	ecs::Entity sheep;

	void Exit() override {
		FileStreamWriter w{ "resources/sheep.bin" };
		w.WriteEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
	}

	void Enter() override {
		camera.primary.SetPosition(game.window.GetCenter());

		game.texture.Load("sheep", "resources/test.png");

		sheep = CreateSheep(manager, V2_float{ 0, 0 });
		if (FileExists("resources/sheep.bin")) {
			FileStreamReader r{ "resources/sheep.bin" };
			r.ReadEntity<Transform, Visible, TextureKey, TargetPosition, Tween>(sheep);
		}
	}
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("Sandbox", { 1280, 720 }, color::Transparent);
	game.scene.Enter<Sandbox>("sandbox");
	return 0;
}
*/

/*
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
// If you can't use C++17's standard library, you'll need to use the GSL
// string_view or implement your own struct (which would not be very difficult,
// since we only need a few methods here)

template <typename T>
constexpr std::string_view type_name();

template <>
constexpr std::string_view type_name<void>() {
	return "void";
}

namespace detail {

using type_name_prober = void;

template <typename T>
constexpr std::string_view wrapped_type_name() {
#ifdef __clang__
	return __PRETTY_FUNCTION__;
#elif defined(__GNUC__)
	return __PRETTY_FUNCTION__;
#elif defined(_MSC_VER)
	return __FUNCSIG__;
#else
#error "Unsupported compiler"
#endif
}

constexpr std::string_view remove_class_or_struct_prefix(std::string_view input) {
	constexpr std::string_view class_prefix	 = "class ";
	constexpr std::string_view struct_prefix = "struct ";

	if (input.size() >= class_prefix.size() &&
		std::equal(class_prefix.begin(), class_prefix.end(), input.begin())) {
		return input.substr(class_prefix.size());
	} else if (input.size() >= struct_prefix.size() &&
			   std::equal(struct_prefix.begin(), struct_prefix.end(), input.begin())) {
		return input.substr(struct_prefix.size());
	} else {
		return input;
	}
}

constexpr std::size_t wrapped_type_name_prefix_length() {
	return wrapped_type_name<type_name_prober>().find(type_name<type_name_prober>());
}

constexpr std::size_t wrapped_type_name_suffix_length() {
	return wrapped_type_name<type_name_prober>().length() - wrapped_type_name_prefix_length() -
		   type_name<type_name_prober>().length();
}

} // namespace detail

template <typename T>
constexpr std::string_view type_name() {
	constexpr auto wrapped_name		= detail::wrapped_type_name<T>();
	constexpr auto prefix_length	= detail::wrapped_type_name_prefix_length();
	constexpr auto suffix_length	= detail::wrapped_type_name_suffix_length();
	constexpr auto type_name_length = wrapped_name.length() - prefix_length - suffix_length;
	return detail::remove_class_or_struct_prefix(
		wrapped_name.substr(prefix_length, type_name_length)
	);
}

std::size_t Hash(std::string_view str) {
	// FNV-1a hash algorithm (cross-compiler consistent)
	std::size_t hash				= 14695981039346656037ULL; // FNV_offset_basis
	constexpr std::size_t FNV_prime = 1099511628211ULL;

	for (char c : str) {
		hash ^= static_cast<std::uint8_t>(c); // XOR with byte
		hash *= FNV_prime;					  // Multiply by prime
	}

	return hash;
}

template <class Base, class... Args>
class Factory {
public:
	virtual ~Factory() = default;

	virtual std::string_view GetName() {
		return name_;
	}

	template <class... T>
	static std::unique_ptr<Base> create(std::string_view class_name, T&&... args) {
		auto it = data().find(Hash(class_name));
		if (it == data().end()) {
			std::cout << "Failed to find hash for " << class_name << std::endl;
		}
		auto ptr{ it->second(std::forward<T>(args)...) };
		ptr->name_ = class_name;
		return ptr;
	}

	template <class T>
	struct Registrar : public Base {
		friend T;

		virtual std::string_view GetName() {
			return type_name<T>();
		}

		static bool registerT() {
			auto raw_name{ type_name<T>() };
			std::cout << "Registering hash for " << raw_name << std::endl;
			const auto name		  = Hash(raw_name);
			Factory::data()[name] = [](Args... args) -> std::unique_ptr<Base> {
				return std::make_unique<T>(std::forward<Args>(args)...);
			};
			return true;
		}

		static bool registered;

		// private:
		Registrar() : Base(Key{}) {
			(void)registered;
		}
	};

	friend Base;

private:
	class Key {
		Key() {};
		template <class T>
		friend struct Registrar;
	};

	using FactoryFuncType = std::unique_ptr<Base> (*)(Args...);
	Factory()			  = default;

	static auto& data() {
		static std::unordered_map<std::size_t, FactoryFuncType> s;
		return s;
	}

	std::string_view name_;
};

template <class Base, class... Args>
template <class T>
bool Factory<Base, Args...>::template Registrar<T>::registered =
	Factory<Base, Args...>::template Registrar<T>::registerT();

struct TweenScript : Factory<TweenScript, int> {
	TweenScript(Key) {}

	virtual ~TweenScript() override = default;

	virtual void OnUpdate(float f) {}
};

template <typename BaseScript, typename TScript>
using Script = typename BaseScript::template Registrar<TScript>;

template <typename TScript>
using TweenScriptClass = Script<TweenScript, TScript>;

class TweenScript1 : public TweenScriptClass<TweenScript1> {
public:
	TweenScript1(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript1: " << e << " updated with " << f << "\n";
	}

private:
	int e{ 0 };
};

class TweenScript2 : public TweenScriptClass<TweenScript1> {
public:
	TweenScript2(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript2: " << e << " updated with " << f << "\n";
	}

private:
	int e{ 0 };
};

template <typename T, typename... Ts>
std::unique_ptr<TweenScript> create_tween_script(Ts&&... args) {
	return std::make_unique<T>(args...);
}

std::unique_ptr<TweenScript> test;

void UpdateTweenScript(float f) {
	if (test) {
		test->OnUpdate(f);
	}
}

template <typename T, typename... Args>
void AddTweenScript(Args... args) {
	test = create_tween_script<T>(args...);
}

template <typename T>
void AddTweenScript(std::unique_ptr<T>&& tween_script) {
	test = std::move(tween_script);
}

std::string_view GetTweenScriptName() {
	return test->GetName();
}

// Do not serialize all components, but instead use a T system.

// TODO: To serialize an entity, write its
// UUID

// Number of components
// (Write zeros equivalent to the above)

// Size of each component.

// To serialize a component, write its
// Component Id
// Component size
// Component data

int main() {
	AddTweenScript<TweenScript1>(10);

	UpdateTweenScript(0.1f);

	std::cout << "Serializing script with name: " << GetTweenScriptName() << std::endl;

	std::string_view from_file{ "TweenScript1" };

	std::cout << "Deserializing script with name: " << from_file << std::endl;

	// TODO: Check if from_file is a TweenScript.
	AddTweenScript(TweenScript::create(from_file, 10));

	UpdateTweenScript(0.9f);

	// std::cout << "Start\n";
	// auto x = TweenScript::create("TweenScript1", 3);
	// auto y = TweenScript::create("TweenScript2", 2);
	// auto x = create_tween_script<TweenScript1>(3);
	// auto y = create_tween_script<TweenScript2>(2);
	// x->OnUpdate(0.1f);
	// y->OnUpdate(0.1f);
	// std::cout << "Name of x for serialization: " << x->GetName() << "\n";
	// std::cout << "Name of y for serialization: " << y->GetName() << "\n";
	// std::cout << "Stop\n";
	return 0;
}
*/

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

#include "serialization/json.h"
#include "utility/macro.h"

// JsonKeyValuePair struct
template <typename T>
struct JsonKeyValuePair {
	std::string_view key;
	T& value;

	JsonKeyValuePair(std::string_view key, T& value) : key(key), value(value) {}
};

// KeyValue function template
template <typename T>
JsonKeyValuePair<T> KeyValue(std::string_view key, T& value) {
	return JsonKeyValuePair<T>(key, value);
}

// Binary Archive classes
class BinaryOutputArchive {
public:
	BinaryOutputArchive(const std::filesystem::path& filePath) :
		os_(filePath, std::ios::binary | std::ios::out) {
		if (!os_.is_open()) {
			throw std::runtime_error(
				"Failed to open binary file for writing: " + filePath.string()
			);
		}
	}

	~BinaryOutputArchive() {
		os_.close(); // Close even if not open (no effect if already closed)
	}

	template <typename T>
	void write(const T& value) {
		if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
			size_t size = value.size();
			os_.write(reinterpret_cast<const char*>(&size), sizeof(size));
			os_.write(value.data(), sizeof(char) * size); // Corrected line
		} else {
			os_.write(reinterpret_cast<const char*>(&value), sizeof(value));
		}
	}

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(write_impl(std::forward<Ts>(values)), ...);
	}

private:
	std::ofstream os_;

	template <typename T>
	void write_impl(T&& value) {
		write(std::forward<T>(value));
	}

	template <typename T>
	void write_impl(JsonKeyValuePair<T> value) { // Overload for JsonKeyValuePair (copy)
		write(value.value);						 // Discard the key, use only the value
	}
};

class BinaryInputArchive {
public:
	BinaryInputArchive(const std::filesystem::path& filePath) :
		is_(filePath, std::ios::binary | std::ios::in) {
		if (!is_.is_open()) {
			throw std::runtime_error(
				"Failed to open binary file for reading: " + filePath.string()
			);
		}
	}

	~BinaryInputArchive() {
		is_.close(); // Close even if not open (no effect if already closed)
	}

	template <typename T>
	void read(T& value) {
		if constexpr (std::is_same_v<std::decay_t<T>, std::string>) {
			size_t size;
			is_.read(reinterpret_cast<char*>(&size), sizeof(size));
			value.resize(size);
			is_.read(value.data(), sizeof(char) * size); // Corrected line
		} else {
			is_.read(reinterpret_cast<char*>(&value), sizeof(value));
		}
	}

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(read_impl(std::forward<Ts>(values)), ...);
	}

private:
	std::ifstream is_;

	template <typename T>
	void read_impl(T&& value) {
		read(std::forward<T>(value));
	}

	template <typename T>
	void read_impl(JsonKeyValuePair<T> value) { // Overload for JsonKeyValuePair (copy)
		read(value.value);						// Discard the key, use only the value
	}
};

// JSON Archive classes
class JsonOutputArchive {
public:
	JsonOutputArchive(const std::filesystem::path& filePath) :
		filePath_(filePath), valueCounter_(0), os_(filePath, std::ios::out) {
		if (!os_.is_open()) {
			throw std::runtime_error("Failed to open json file for writing: " + filePath.string());
		}
	}

	~JsonOutputArchive() {
		os_.close(); // Close even if not open (no effect if already closed)
		if (!filePath_.empty() && !jsonData_.empty()) {
			std::ofstream temp_os(filePath_, std::ios::out);
			temp_os << jsonData_.dump(4);
		}
	}

	template <typename T>
	void write(const T& value, std::string_view key) {
		jsonData_[key] = value;
	}

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(write_json_impl(std::forward<Ts>(values)), ...);
	}

private:
	std::ofstream os_;
	nlohmann::json jsonData_;
	std::filesystem::path filePath_;
	int valueCounter_;

	template <typename T>
	void write_json_impl(T&& value) {
		write(std::forward<T>(value), "value" + std::to_string(valueCounter_++));
	}

	template <typename T>
	void write_json_impl(JsonKeyValuePair<T> pair) {
		write(pair.value, pair.key);
	}
};

class JsonInputArchive {
public:
	JsonInputArchive(const std::filesystem::path& filePath) :
		valueCounter_(0), is_(filePath, std::ios::in) {
		if (!is_.is_open()) {
			throw std::runtime_error("Failed to open json file for reading: " + filePath.string());
		}
		is_ >> jsonData_;
	}

	~JsonInputArchive() {
		is_.close(); // Close even if not open (no effect if already closed)
	}

	template <typename T>
	void read(T& value, std::string_view key) {
		value = jsonData_[key].get<T>();
	}

	template <typename... Ts>
	void operator()(Ts&&... values) {
		(read_json_impl(std::forward<Ts>(values)), ...);
	}

private:
	std::ifstream is_;
	nlohmann::json jsonData_;
	int valueCounter_;

	template <typename T>
	void read_json_impl(T&& value) {
		read(std::forward<T>(value), "value" + std::to_string(valueCounter_++));
	}

	template <typename T>
	void read_json_impl(JsonKeyValuePair<T> pair) {
		read(pair.value, pair.key);
	}
};

#define PTGN_ARCHIVE_ONE(archive, x) archive(KeyValue(#x, x));

#define PTGN_SERIALIZER_REGISTER(...)         \
	template <typename Archive>               \
	void serialize(Archive& archive) const {} \
	template <typename Archive>               \
	void deserialize(Archive& archive) {}

// MyData class with template Serialize/Deserialize
class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	// PTGN_SERIALIZER_REGISTER(id, message, value)
};

#define EXPAND(x) x
#define DEF_AUX_NARGS(                                                                             \
	x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15, x16, x17, x18, x19, x20,     \
	x21, x22, x23, x24, x25, x26, x27, x28, x29, x30, x31, x32, x33, x34, x35, x36, x37, x38, x39, \
	x40, x41, x42, x43, x44, x45, x46, x47, x48, x49, x50, x51, x52, x53, x54, x55, x56, x57, x58, \
	x59, x60, x61, x62, x63, x64, VAL, ...                                                         \
)                                                                                                  \
	VAL
#define NARGS(...)                                                                               \
	EXPAND(DEF_AUX_NARGS(                                                                        \
		__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, \
		45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24,  \
		23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0     \
	))

// --------------------------------------------------
#define FE0_1(what, x)		EXPAND(what(x))
#define FE0_2(what, x, ...) EXPAND(what(x) FE0_1(what, __VA_ARGS__))
#define FE0_3(what, x, ...) EXPAND(what(x) FE0_2(what, __VA_ARGS__))
#define FE0_4(what, x, ...) EXPAND(what(x) FE0_3(what, __VA_ARGS__))
#define FE0_5(what, x, ...) EXPAND(what(x) FE0_4(what, __VA_ARGS__))
#define FE0_6(what, x, ...) EXPAND(what(x) FE0_5(what, __VA_ARGS__))
#define FE0_7(what, x, ...) EXPAND(what(x) FE0_6(what, __VA_ARGS__))
#define FE0_8(what, x, ...) EXPAND(what(x) FE0_7(what, __VA_ARGS__))

#define REPEAT0(...) \
	EXPAND(DEF_AUX_NARGS(__VA_ARGS__, FE0_8, FE0_7, FE0_6, FE0_5, FE0_4, FE0_3, FE0_2, FE0_1, 0))
#define FOR_EACH(what, ...) EXPAND(REPEAT0(__VA_ARGS__)(what, __VA_ARGS__))

// --------------------------------------------------
#define FE1_1(what, x, y)	   EXPAND(what(x, y))
#define FE1_2(what, x, y, ...) EXPAND(what(x, y) FE1_1(what, x, __VA_ARGS__))
#define FE1_3(what, x, y, ...) EXPAND(what(x, y) FE1_2(what, x, __VA_ARGS__))
#define FE1_4(what, x, y, ...) EXPAND(what(x, y) FE1_3(what, x, __VA_ARGS__))
#define FE1_5(what, x, y, ...) EXPAND(what(x, y) FE1_4(what, x, __VA_ARGS__))
#define FE1_6(what, x, y, ...) EXPAND(what(x, y) FE1_5(what, x, __VA_ARGS__))
#define FE1_7(what, x, y, ...) EXPAND(what(x, y) FE1_6(what, x, __VA_ARGS__))
#define FE1_8(what, x, y, ...) EXPAND(what(x, y) FE1_7(what, x, __VA_ARGS__))

#define REPEAT1(...) \
	EXPAND(DEF_AUX_NARGS(__VA_ARGS__, FE1_8, FE1_7, FE1_6, FE1_5, FE1_4, FE1_3, FE1_2, FE1_1, 0))
#define FOR_EACH_PIVOT_1ST_ARG(what, arg0, ...) \
	EXPAND(REPEAT1(__VA_ARGS__)(what, arg0, __VA_ARGS__))

// --------------------------------------------------
#define APPLY1(FN, x, y)	   EXPAND(FN(x, y))
#define APPLY2(FN, x, y, ...)  EXPAND(FN(x, y) APPLY1(FN, __VA_ARGS__))
#define APPLY3(FN, x, y, ...)  EXPAND(FN(x, y) APPLY2(FN, __VA_ARGS__))
#define APPLY4(FN, x, y, ...)  EXPAND(FN(x, y) APPLY3(FN, __VA_ARGS__))
#define APPLY5(FN, x, y, ...)  EXPAND(FN(x, y) APPLY4(FN, __VA_ARGS__))
#define APPLY6(FN, x, y, ...)  EXPAND(FN(x, y) APPLY5(FN, __VA_ARGS__))
#define APPLY7(FN, x, y, ...)  EXPAND(FN(x, y) APPLY6(FN, __VA_ARGS__))
#define APPLY8(FN, x, y, ...)  EXPAND(FN(x, y) APPLY7(FN, __VA_ARGS__))
#define APPLY9(FN, x, y, ...)  EXPAND(FN(x, y) APPLY8(FN, __VA_ARGS__))
#define APPLY10(FN, x, y, ...) EXPAND(FN(x, y) APPLY9(FN, __VA_ARGS__))
#define APPLY11(FN, x, y, ...) EXPAND(FN(x, y) APPLY10(FN, __VA_ARGS__))
#define APPLY12(FN, x, y, ...) EXPAND(FN(x, y) APPLY11(FN, __VA_ARGS__))
#define APPLY13(FN, x, y, ...) EXPAND(FN(x, y) APPLY12(FN, __VA_ARGS__))
#define APPLY14(FN, x, y, ...) EXPAND(FN(x, y) APPLY13(FN, __VA_ARGS__))
#define APPLY15(FN, x, y, ...) EXPAND(FN(x, y) APPLY14(FN, __VA_ARGS__))
#define APPLY16(FN, x, y, ...) EXPAND(FN(x, y) APPLY15(FN, __VA_ARGS__))
#define APPLY17(FN, x, y, ...) EXPAND(FN(x, y) APPLY16(FN, __VA_ARGS__))
#define APPLY18(FN, x, y, ...) EXPAND(FN(x, y) APPLY17(FN, __VA_ARGS__))
#define APPLY19(FN, x, y, ...) EXPAND(FN(x, y) APPLY18(FN, __VA_ARGS__))
#define APPLY20(FN, x, y, ...) EXPAND(FN(x, y) APPLY19(FN, __VA_ARGS__))
#define APPLY21(FN, x, y, ...) EXPAND(FN(x, y) APPLY20(FN, __VA_ARGS__))
#define APPLY22(FN, x, y, ...) EXPAND(FN(x, y) APPLY21(FN, __VA_ARGS__))
#define APPLY23(FN, x, y, ...) EXPAND(FN(x, y) APPLY22(FN, __VA_ARGS__))
#define APPLY24(FN, x, y, ...) EXPAND(FN(x, y) APPLY23(FN, __VA_ARGS__))
#define APPLY25(FN, x, y, ...) EXPAND(FN(x, y) APPLY24(FN, __VA_ARGS__))
#define APPLY26(FN, x, y, ...) EXPAND(FN(x, y) APPLY25(FN, __VA_ARGS__))
#define APPLY27(FN, x, y, ...) EXPAND(FN(x, y) APPLY26(FN, __VA_ARGS__))
#define APPLY28(FN, x, y, ...) EXPAND(FN(x, y) APPLY27(FN, __VA_ARGS__))
#define APPLY29(FN, x, y, ...) EXPAND(FN(x, y) APPLY28(FN, __VA_ARGS__))
#define APPLY30(FN, x, y, ...) EXPAND(FN(x, y) APPLY29(FN, __VA_ARGS__))
#define APPLY31(FN, x, y, ...) EXPAND(FN(x, y) APPLY30(FN, __VA_ARGS__))
#define APPLY32(FN, x, y, ...) EXPAND(FN(x, y) APPLY31(FN, __VA_ARGS__))

#define NPAIRARGS(...)                                                                           \
	EXPAND(DEF_AUX_NARGS(                                                                        \
		__VA_ARGS__, 32, 32, 31, 31, 30, 30, 29, 29, 28, 28, 27, 27, 26, 26, 25, 25, 24, 24, 23, \
		23, 22, 22, 21, 21, 20, 20, 19, 19, 18, 18, 17, 17, 16, 16, 15, 15, 14, 14, 13, 13, 12,  \
		12, 11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1                 \
	))
#define APPLYNPAIRARGS(...)                                                                       \
	EXPAND(DEF_AUX_NARGS(                                                                         \
		__VA_ARGS__, APPLY32, APPLY32, APPLY31, APPLY31, APPLY30, APPLY30, APPLY29, APPLY29,      \
		APPLY28, APPLY28, APPLY27, APPLY27, APPLY26, APPLY26, APPLY25, APPLY25, APPLY24, APPLY24, \
		APPLY23, APPLY23, APPLY22, APPLY22, APPLY21, APPLY21, APPLY20, APPLY20, APPLY19, APPLY19, \
		APPLY18, APPLY18, APPLY17, APPLY17, APPLY16, APPLY16, APPLY15, APPLY15, APPLY14, APPLY14, \
		APPLY13, APPLY13, APPLY12, APPLY12, APPLY11, APPLY11, APPLY10, APPLY10, APPLY9, APPLY9,   \
		APPLY8, APPLY8, APPLY7, APPLY7, APPLY6, APPLY6, APPLY5, APPLY5, APPLY4, APPLY4, APPLY3,   \
		APPLY3, APPLY2, APPLY2, APPLY1, APPLY1                                                    \
	))
#define FOR_EACH_PAIR(FN, ...) EXPAND(APPLYNPAIRARGS(__VA_ARGS__)(FN, __VA_ARGS__))

#define TESTING_123(x, y) (std::cout << #x << "(" << #y << ")\n");

int main() {
	MyData data1;
	data1.id	  = 123;
	data1.message = "Binary Data";
	data1.value	  = 3.14f;

	FOR_EACH_PIVOT_1ST_ARG(TESTING_123, "archive", "hi", "hello");

	/*
	// Binary Serialization
	{
		BinaryOutputArchive binaryOutputArchive("resources/mydata.bin");
		MyData data1;
		data1.id	  = 123;
		data1.message = "Binary Data";
		data1.value	  = 3.14f;

		data1.serialize(binaryOutputArchive);
	}

	// Binary Deserialization
	{
		BinaryInputArchive binaryInputArchive("resources/mydata.bin");
		MyData data2;
		data2.deserialize(binaryInputArchive);

		std::cout << "Binary: id=" << data2.id << ", message=\"" << data2.message
				  << "\", value=" << data2.value << std::endl;
	}

	// JSON Serialization
	{
		JsonOutputArchive jsonOutputArchive("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		data3.serialize(jsonOutputArchive);
	}

	// JSON Deserialization
	{
		JsonInputArchive jsonInputArchive("resources/mydata.json");
		MyData data4;
		data4.deserialize(jsonInputArchive);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}
	*/

	return 0;
}