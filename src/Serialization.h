#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip> // pretty JSON printing
#include <nlohmann/json.hpp>

#include "Utilities.h"

using json = nlohmann::json;

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