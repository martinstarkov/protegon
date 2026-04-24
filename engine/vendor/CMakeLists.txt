include_guard(GLOBAL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# native file dialog
if (NOT EMSCRIPTEN)
add_subdirectory(nativefiledialog-extended)
endif()

# magic_enum
add_subdirectory(magic_enum)

# GLFW
if(EMSCRIPTEN)
  # Use Emscripten's built-in GLFW port.
  add_library(glfw INTERFACE)
  target_link_options(glfw INTERFACE "-sUSE_GLFW=3")
else()
  set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
  add_subdirectory(glfw)
endif()

# glad
if(EMSCRIPTEN)
  # Emscripten provides GL/WebGL entry points; no loader needed.
  add_library(ptgn_glad INTERFACE)
else()
  add_library(ptgn_glad STATIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/src/gl.c)
  target_include_directories(ptgn_glad PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/glad/include)
endif()

# Ogg / Vorbis
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

set(_old_warn_deprecated ${CMAKE_WARN_DEPRECATED})
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
add_subdirectory(ogg)
add_subdirectory(vorbis)
set(CMAKE_WARN_DEPRECATED ${_old_warn_deprecated} CACHE BOOL "" FORCE)

# miniaudio
add_library(ptgn_miniaudio STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/miniaudio.c
  ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/extras/decoders/libvorbis/miniaudio_libvorbis.c
)

target_include_directories(ptgn_miniaudio
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/extras/decoders/libvorbis
    ${CMAKE_CURRENT_SOURCE_DIR}/ogg/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vorbis/include
)

target_link_libraries(ptgn_miniaudio
  PUBLIC
    ogg
    vorbis
    vorbisfile
)

# stb
add_library(ptgn_stb STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb/stb.cpp)

target_include_directories(ptgn_stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)

# nlohmann_json
add_subdirectory(json)

# ecs
add_library(ptgn_ecs INTERFACE)
target_include_directories(ptgn_ecs INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/ecs/include)

# Dear ImGui
# GLFW + OpenGL3 backend
add_library(ptgn_imgui STATIC
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_draw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_tables.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/imgui_widgets.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp/imgui_stdlib.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_glfw.cpp
  ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends/imgui_impl_opengl3.cpp
)

target_include_directories(ptgn_imgui
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/imgui/misc/cpp
)

target_link_libraries(ptgn_imgui PUBLIC glfw)

if(EMSCRIPTEN)
  target_compile_definitions(ptgn_imgui PUBLIC IMGUI_IMPL_OPENGL_ES3)
endif()

# FreeType
set(FT_DISABLE_ZLIB TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ TRUE CACHE BOOL "" FORCE)
set(FT_DISABLE_BROTLI TRUE CACHE BOOL "" FORCE)

add_subdirectory(freetype)

if(NOT TARGET Freetype::Freetype AND TARGET freetype)
  add_library(Freetype::Freetype ALIAS freetype)
endif()

# msdf-atlas-gen
set(MSDFGEN_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_BUILD_STANDALONE OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_VCPKG OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_USE_SKIA OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_NO_ARTERY_FONT ON CACHE BOOL "" FORCE)
set(MSDF_ATLAS_MSDFGEN_EXTERNAL OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_INSTALL OFF CACHE BOOL "" FORCE)
set(MSDF_ATLAS_DYNAMIC_RUNTIME OFF CACHE BOOL "" FORCE)

add_subdirectory(msdf-atlas-gen)

function(set_folder_if_target folder)
  foreach(t IN LISTS ARGN)
    if(TARGET "${t}")
      set_target_properties("${t}" PROPERTIES FOLDER "${folder}")
    endif()
  endforeach()
endfunction()

set_folder_if_target("vendor"
  glfw
  ptgn_glad
  ptgn_imgui
  ptgn_miniaudio
  ptgn_stb
  ptgn_imgui_obj
  vorbis
  vorbisenc
  vorbisfile
  ogg
  ptgn_ecs
  freetype
  msdf-atlas-gen
  msdfgen-core
  msdfgen-ext
  nfd
)

set_folder_if_target("vendor/GLFW3" update_mappings)

foreach(tgt msdfgen-core msdfgen-ext msdf-atlas-gen)
  if(TARGET ${tgt})
    set_property(TARGET ${tgt} PROPERTY
      MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
  endif()
endforeach()