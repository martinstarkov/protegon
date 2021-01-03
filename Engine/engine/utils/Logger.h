#pragma once

#include <iostream> // std::cout, std::endl

// TODO: Eventually move logging into class which can print to console and log to file.

#define LOG(x) { std::cout << x << std::endl; }
#define LOG_(x) { std::cout << x; }