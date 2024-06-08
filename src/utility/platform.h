#pragma once

// Source: https://github.com/TheCherno/Hazel/blob/master/Hazel/src/Hazel/Core/PlatformDetection.h

// This file defines PTGN_PLATFORM_WINDOWS, PTGN_PLATFORM_MACOS, or PTGN_PLATFORM_LINUX

#ifdef _WIN32
	#ifdef _WIN64 /* Windows x64/x86 */
		#define PTGN_PLATFORM_WINDOWS /* Windows x64  */
	#else
		#error "x86 Builds are not supported!" /* Windows x86 */
	#endif
#elif defined(__APPLE__) || defined(__MACH__)
	#include <TargetConditionals.h>
	/* TARGET_OS_MAC exists on all the platforms
	 * so we must check all of them (in this order)
	 * to ensure that we're running on MAC
	 * and not some other Apple platform */
	#if TARGET_IPHONE_SIMULATOR == 1
		#error "IOS simulator is not supported!"
	#elif TARGET_OS_IPHONE == 1
		#define PTGN_PLATFORM_IOS
		#error "IOS is not supported!"
	#elif TARGET_OS_MAC == 1
		#define PTGN_PLATFORM_MACOS
	#else
		#error "Unknown Apple platform!"
	#endif
/* We also have to check __ANDROID__ before __linux__
 * since android is based on the linux kernel
 * it has __linux__ defined */
#elif defined(__ANDROID__)
	#define PTGN_PLATFORM_ANDROID
	#error "Android is not supported!"
#elif defined(__linux__)
	#define PTGN_PLATFORM_LINUX
#else
	/* Unknown compiler/platform */
	#error "Unknown compiler/platform!"
#endif