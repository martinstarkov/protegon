#pragma once

struct Rectangle {
	Rectangle(int x = 0, int y = 0, int w = 0, int h = 0) : x(x), y(y), w(w), h(h) {}
	int x, y, w, h;
};