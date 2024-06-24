
set(SDL2_VERSION       2.30.0)
set(SDL2_IMAGE_VERSION  2.8.2)
set(SDL2_TTF_VERSION   2.22.0)
set(SDL2_MIXER_VERSION  2.8.0)

if (WIN32 AND DOWNLOAD_SDL)

set(SDL_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/external/sdl2-msvc-shared")

list(APPEND CMAKE_PREFIX_PATH "${SDL_LOCATION}/SDL2-${SDL2_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_LOCATION}/SDL2_image-${SDL2_IMAGE_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_LOCATION}/SDL2_ttf-${SDL2_TTF_VERSION}/")
list(APPEND CMAKE_PREFIX_PATH "${SDL_LOCATION}/SDL2_mixer-${SDL2_MIXER_VERSION}/")

endif()

set(SDL2MAIN_LIBRARY FALSE)

find_package(SDL2        ${SDL2_VERSION}       REQUIRED)
find_package(SDL2_image  ${SDL2_IMAGE_VERSION} REQUIRED)
find_package(SDL2_ttf    ${SDL2_TTF_VERSION}   REQUIRED)
find_package(SDL2_mixer  ${SDL2_MIXER_VERSION} REQUIRED)

# Copy SDL dlls to target executable directory
if(WIN32)

get_target_property(SDL2_DLL       SDL2::SDL2 	        IMPORTED_LOCATION)
get_target_property(SDL2_IMAGE_DLL SDL2_image::SDL2_image IMPORTED_LOCATION)
get_target_property(SDL2_TTF_DLL   SDL2_ttf::SDL2_ttf     IMPORTED_LOCATION)
get_target_property(SDL2_MIXER_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)

get_filename_component(SDL2_LIB_DIR       ${SDL2_DLL}       DIRECTORY)
get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_DLL} DIRECTORY)
get_filename_component(SDL2_TTF_LIB_DIR   ${SDL2_TTF_DLL}   DIRECTORY)
get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_DLL} DIRECTORY)

file(GLOB_RECURSE SDL_DLLS CONFIGURE_DEPENDS
     ${SDL2_LIB_DIR}/*.dll
     ${SDL2_IMAGE_LIB_DIR}/*.dll
     ${SDL2_TTF_LIB_DIR}/*.dll
     ${SDL2_MIXER_LIB_DIR}/*.dll)

if("${SDL_DLLS}" STREQUAL "")
message(FATAL_ERROR "Could not find SDL2 dlls")
endif()

set(SDL_DLLS ${SDL_DLLS} CACHE BOOL "")

function(add_sdl_dll_copy TARGET)

add_custom_command(TARGET ${TARGET} POST_BUILD COMMAND ${CMAKE_COMMAND}
                   -E copy_if_different ${SDL_DLLS} $<TARGET_FILE_DIR:${TARGET}>
                   COMMAND_EXPAND_LISTS)

endfunction()

endif()

#target_link_libraries(protegon PRIVATE ${SDL_LIBRARIES})

# if(WIN32)
#   if (MINGW)
#     target_link_libraries(protegon PUBLIC shlwapi)
#     if (NOT BUILD_SHARED_LIBS)
#       target_link_options(protegon PUBLIC "-static")
#     endif()
#   elseif (MSVC)
#     if (NOT SHARED_SDL2_LIBS)
#       set(FREETYPE_LIB "${OUTPUT_DIR}/${SDL2_TTF_NAME}/lib/${CMAKE_LIBRARY_ARCHITECTURE}/freetype.lib")
#       target_link_libraries(protegon PRIVATE ${FREETYPE_LIB})
#     endif()
#     target_link_options(protegon PUBLIC $<IF:$<CONFIG:Debug>,/NODEFAULTLIB:MSVCRT,>)
#   endif()
#   if(NOT SHARED_SDL2_LIBS AND ${DOWNLOAD_SDL2} AND CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
#     function(get_target_file_paths TARGETS OUTPUT_VAR)
#       set(TARGET_FILE_PATHS)
#       foreach(TARGET IN LISTS ${TARGETS})
#         list(APPEND TARGET_FILE_PATHS $<TARGET_FILE:${TARGET}>)
#       endforeach(TARGET)
#       set(${OUTPUT_VAR} ${TARGET_FILE_PATHS} PARENT_SCOPE)
#     endfunction(get_target_file_paths)

#     get_target_file_paths(SDL_LIBRARIES SDL_TARGET_FILES)
#     set(SDL_TARGET_FILES "${SDL_TARGET_FILES}" CACHE INTERNAL "")
#     if(MSVC)
#       list(APPEND SDL_TARGET_FILES ${FREETYPE_LIB})
#       add_custom_command(TARGET protegon POST_BUILD
#                         COMMAND ${CMAKE_AR} /NOLOGO /OUT:"$<TARGET_FILE:protegon>" "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
#                         COMMAND_EXPAND_LISTS)
#     elseif(MINGW)
#       add_custom_command(TARGET protegon POST_BUILD
#                         COMMAND ${CMAKE_AR}
#                         VERBATIM r "$<TARGET_FILE:protegon>" "${SDL_TARGET_FILES}"
#                         COMMAND_EXPAND_LISTS)
#     endif()
#   elseif(SHARED_SDL2_LIBS)
#     # Copy SDL dlls to parent exe directory
#     get_target_property(SDL2_DLL       SDL2::SDL2 	          IMPORTED_LOCATION)
#     get_target_property(SDL2_IMAGE_DLL SDL2_image::SDL2_image IMPORTED_LOCATION)
#     get_target_property(SDL2_TTF_DLL   SDL2_ttf::SDL2_ttf     IMPORTED_LOCATION)
#     get_target_property(SDL2_MIXER_DLL SDL2_mixer::SDL2_mixer IMPORTED_LOCATION)

#     get_filename_component(SDL2_LIB_DIR       ${SDL2_DLL}       DIRECTORY)
#     get_filename_component(SDL2_IMAGE_LIB_DIR ${SDL2_IMAGE_DLL} DIRECTORY)
#     get_filename_component(SDL2_TTF_LIB_DIR   ${SDL2_TTF_DLL}   DIRECTORY)
#     get_filename_component(SDL2_MIXER_LIB_DIR ${SDL2_MIXER_DLL} DIRECTORY)

#     file(GLOB_RECURSE DLLS CONFIGURE_DEPENDS
#       ${SDL2_LIB_DIR}/*.dll
#       ${SDL2_IMAGE_LIB_DIR}/*.dll
#       ${SDL2_TTF_LIB_DIR}/*.dll
#       ${SDL2_MIXER_LIB_DIR}/*.dll)

#     set(PTGN_SDL_DLLS "${DLLS}" CACHE INTERNAL "")
#   endif()
# endif()
