#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>

#include <nlohmann/json.hpp>

#include "Utilities.h"

#include "SDL.h"

using json = nlohmann::json;

// partial specialization for SDL types
namespace nlohmann {
	template <>
	struct adl_serializer<SDL_Color> {
		static void to_json(json& j, const SDL_Color& o) {
			j["r"] = o.r;
			j["g"] = o.g;
			j["b"] = o.b;
			j["a"] = o.a;
		}
		static void from_json(const json& j, SDL_Color& o) {
			if (j.find("r") != j.end() || j.find("g") != j.end() || j.find("b") != j.end() || j.find("a") != j.end()) {
				o = { 0, 0, 0, 0 };
			}
			if (j.find("r") != j.end()) {
				o.r = j.at("r").get<Uint8>();
			}
			if (j.find("g") != j.end()) {
				o.g = j.at("g").get<Uint8>();
			}
			if (j.find("b") != j.end()) {
				o.b = j.at("b").get<Uint8>();
			}
			if (j.find("a") != j.end()) {
				o.a = j.at("a").get<Uint8>();
			}
		}
	};
	template <>
	struct adl_serializer<SDL_Rect> {
		static void to_json(json& j, const SDL_Rect& o) {
			j["x"] = o.x;
			j["y"] = o.y;
			j["w"] = o.w;
			j["h"] = o.h;
		}
		static void from_json(const json& j, SDL_Rect& o) {
			if (j.find("x") != j.end() || j.find("y") != j.end() || j.find("w") != j.end() || j.find("h") != j.end()) {
				o = { 0, 0, 0, 0 };
			}
			if (j.find("x") != j.end()) {
				o.x = j.at("x").get<int>();
			}
			if (j.find("y") != j.end()) {
				o.y = j.at("y").get<int>();
			}
			if (j.find("w") != j.end()) {
				o.w = j.at("w").get<int>();
			}
			if (j.find("h") != j.end()) {
				o.h = j.at("h").get<int>();
			}
		}
	};
}

class Serialization {
public:
	template <typename T>
	static void serialize(std::string path, T& obj) {
		std::ofstream out(path);
		json j;
		j[typeid(obj).name()] = obj;
		out << std::setw(4) << j; // overloaded setw for pretty printing, number is amount of spaces to indent
		out.close();
	}
	template <typename T>
	static void serialize(std::string path, T* obj) {
		assert(obj && "Cannot serialize nullptr object");
		serialize(path, *obj);
	}
	template <typename T>
	static void reserialize(std::string path, T& obj) {
		std::remove(path.c_str());
		serialize(path, obj);
	}
	template <typename T>
	static void reserialize(std::string path, T* obj) {
		assert(obj && "Cannot reserialize nullptr object");
		reserialize(path, *obj);
	}
	template <typename T>
	static void deserialize(std::string path, T& obj) {
		std::ifstream in(path);
		json j;
		in >> j;
		// write tests
		obj = j[typeid(obj).name()].get<T>();
		in.close();
	}
	template <typename T>
	static void deserialize(std::string path, T* obj) {
		assert(obj && "Cannot deserialize to a nullptr");
		reserialize(path, *obj);
	}
	template <typename T>
	static T deserialize(std::string path) {
		T t;
		deserialize(path, t);
		return t;
	}
};