/*
#include "core/game.h"
#include "core/game_object.h"
#include "core/window.h"
#include "ecs/ecs.h"
#include "event/input_handler.h"
#include "event/key.h"
#include "math/collision/collider.h"
#include "physics/movement.h"
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
		Key(){};
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
bool Factory<Base, Args...>::Registrar<T>::registered =
	Factory<Base, Args...>::Registrar<T>::registerT();

struct TweenScript : Factory<TweenScript, int> {
	TweenScript(Key) {}

	virtual ~TweenScript() override = default;

	virtual void OnUpdate(float f) {}
};

class TweenScript1 : public TweenScript::Registrar<TweenScript1> {
public:
	TweenScript1(int e) : e(e) {}

	void OnUpdate(float f) override {
		std::cout << "TweenScript1: " << e << " updated with " << f << "\n";
	}

private:
	int e{ 0 };
};

class TweenScript2 : public TweenScript::Registrar<TweenScript2> {
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

int main() {
	AddTweenScript<TweenScript1>(10);

	UpdateTweenScript(0.1f);

	std::cout << "Serializing script with name: " << GetTweenScriptName() << std::endl;

	std::string_view from_file{ "TweenScript1" };

	std::cout << "Deserializing script with name: " << from_file << std::endl;

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
