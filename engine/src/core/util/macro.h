#pragma once

#define PTGN_EXPAND(x)	  x
#define PTGN_STRINGIFY(x) #x
#define PTGN_UNPAREN(...) __VA_ARGS__

#define PTGN_IMPL_FIRST_OR_DEFAULT_IMPL(value, ...) value

#define PTGN_IMPL_FIRST_OR_DEFAULT(default_value, ...) PTGN_IMPL_FIRST_OR_DEFAULT_IMPL(__VA_ARGS__ __VA_OPT__(, ) default_value)