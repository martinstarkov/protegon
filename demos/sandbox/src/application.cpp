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
#include <tuple>
#include <type_traits>
#include <utility>

#include "serialization/binary_archive.h"
#include "serialization/json_archive.h"
#include "serialization/serializable.h"

using namespace ptgn;

/*
// JSON Archive classes
class JsonOutputArchive {
public:
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
	json jsonData_;
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
*/

class MyData {
public:
	MyData() : id(0), message(""), value(0.0f) {}

	int id;
	std::string message;
	float value;

	PTGN_SERIALIZER_REGISTER(id, message, value)
};

int main() {
	static_assert(has_template_function_Serialize_v<MyData>);
	static_assert(has_template_function_Deserialize_v<MyData>);
	// Binary Serialization
	{
		BinaryOutputArchive binary_output("resources/mydata.bin");
		MyData data1;
		data1.id	  = 123;
		data1.message = "Binary Data";
		data1.value	  = 3.14f;
		binary_output.Write(data1);
	}

	// Binary Deserialization
	{
		BinaryInputArchive binary_input("resources/mydata.bin");
		MyData data2;
		binary_input.Read(data2);

		std::cout << "Binary: id=" << data2.id << ", message=\"" << data2.message
				  << "\", value=" << data2.value << std::endl;
	}

	// JSON Serialization
	{
		JsonOutputArchive json_output("resources/mydata.json");
		MyData data3;
		data3.id	  = 456;
		data3.message = "JSON Data";
		data3.value	  = 2.71f;

		json_output.Write("data3", data3);
	}

	// JSON Deserialization
	{
		JsonInputArchive json_input("resources/mydata.json");
		MyData data4;

		json_input.Read("data3", data4);

		std::cout << "JSON: id=" << data4.id << ", message=\"" << data4.message
				  << "\", value=" << data4.value << std::endl;
	}

	return 0;
}