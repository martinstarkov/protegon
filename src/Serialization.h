#pragma once

#include <iostream>
#include <fstream>
#include <string>
#include <functional>

#define SERIALIZATION_SEPARATOR ":"
#define SERIALIZATION_OPEN "{"
#define SERIALIZATION_CLOSE "}"

template <typename T>
static void serialize(T* obj, std::ostream& (T::*serializeFunction)(std::ostream& out), const char* path) {
	std::ofstream out(path);
	assert(out.good() && "Cannot serialize file with invalid path");
	assert(obj && "Cannot call serialize() method on nullptr");
	(obj->*serializeFunction)(out);
	out.close();
}

template <typename T>
static void reserialize(T* obj, std::ostream& (T::*serializeFunction)(std::ostream& out), const char* path) {
	std::remove(path);
	serialize(obj, serializeFunction, path);
}

template <typename T>
static T deserialize(T (*deserializeFunction)(std::istream& in), const char* path) {
	std::ifstream in(path);
	assert(in.good() && "Cannot deserialize file with invalid path");
	T t(deserializeFunction(in));
	in.close();
	return t;
}

template <typename T>
static void deserialize(T& obj, T(*deserializeFunction)(std::istream& in), const char* path) {
	obj = deserialize(deserializeFunction, path);
}