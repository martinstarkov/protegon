
#include "core/game.h"
#include "scene/scene.h"
#include "scene/scene_manager.h"

using namespace ptgn;

class BinarySerializationScene : public Scene {
public:
	// TODO: Fix.
	/*
	struct Trivial {
		int a{ 0 };
		int b{ 0 };
	};

	struct TrivialComposite {
		Trivial t;
	};

	struct NonTrivial {
		std::vector<int> v;

		template <typename... Ts>
		static void Serialize(StreamWriter* w, const NonTrivial& nt) {
			w->Write(nt.v);
		}

		template <typename... Ts>
		static void Deserialize(StreamReader* r, NonTrivial& nt) {
			r->Read(nt.v);
		}
	};

	template <typename T>
	void PrintContainer(std::string_view name, const T& container) {
		Print(name, ": ");
		for (const auto& v : container) {
			Print(v, ", ");
		}
		PrintLine();
	};

	template <typename T>
	void PrintMap(std::string_view name, const T& map) {
		Print(name, ": ");
		for (const auto& [k, v] : map) {
			Print("{ ", k, ", ", v, " }, ");
		}
		PrintLine();
	};

	void Enter() override {
		static_assert(std::is_trivially_copyable_v<Trivial>);
		static_assert(std::is_trivially_copyable_v<TrivialComposite>);

		{
			Trivial trivial{ 42, 69 };
			TrivialComposite trivial_composite{ Trivial{ 43, 70 } };
			NonTrivial serializable{ std::vector<int>{ 1, 2, 3 } };
			std::string string{ "Hello world!" };
			std::vector<int> vector{ 4, 5, 6 };
			std::array<int, 3> array{ 7, 8, 9 };
			std::map<int, int> map{ { 10, 11 }, { 12, 13 }, { 14, 15 } };
			std::unordered_map<int, int> unordered_map{ { 16, 17 }, { 18, 19 }, { 20, 21 } };

			FileStreamWriter w{ "resources/data.bin" };

			w.Write(trivial);
			w.Write(trivial_composite);
			w.Write(serializable);
			w.Write(string);
			w.Write(vector);
			w.Write(array);
			w.Write(map);
			w.Write(unordered_map);
		}

		{
			Trivial trivial;
			TrivialComposite trivial_composite;
			NonTrivial deserializable;
			std::string string;
			std::vector<int> vector;
			std::array<int, 3> array;
			std::map<int, int> map;
			std::unordered_map<int, int> unordered_map;

			FileStreamReader r{ "resources/data.bin" };

			auto print_values = [&]() {
				PTGN_LOG("trivial: ", trivial.a, ", ", trivial.b);
				PTGN_LOG("trivial_composite: ", trivial_composite.t.a, ", ", trivial_composite.t.b);
				PrintContainer("deserializable", deserializable.v);
				PTGN_LOG("string: ", string);
				PrintContainer("vector", vector);
				PrintContainer("array", array);
				PrintMap("map", map);
				PrintMap("unordered_map", unordered_map);
			};

			PTGN_LOG("Before read: ");

			print_values();

			r.Read(trivial);
			r.Read(trivial_composite);
			r.Read(deserializable);
			r.Read(string);
			r.Read(vector);
			r.Read(array);
			r.Read(map);
			r.Read(unordered_map);

			PTGN_LOG("After read: ");

			print_values();
		}

		game.Stop();
	}
*/
};

int main([[maybe_unused]] int c, [[maybe_unused]] char** v) {
	game.Init("BinarySerializationScene", { 1280, 720 });
	game.scene.Enter<BinarySerializationScene>("");
	return 0;
}