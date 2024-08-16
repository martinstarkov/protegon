cmake_minimum_required(VERSION 3.20)

project(tests)

add_executable(${PROJECT_NAME} ${PROTEGON_TEST_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE "${PROTEGON_ROOT_DIR}/src")

add_protegon_to(${PROJECT_NAME})

if (EMSCRIPTEN)
    set(ASSETS_DIRECTORY "resources")
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${ASSETS_DIRECTORY}")
        create_resource_symlink(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}../../../ ${ASSETS_DIRECTORY})
    endif()
    set(SHELL_HTML_FILE "${PROTEGON_ROOT_DIR}/emscripten/shell.html")
    set(CMAKE_EXECUTABLE_SUFFIX ".html") 
    set(ECXXFLAGS "-O3 -std=c++17 --use-port=sdl2 --use-port=sdl2_image:formats=bmp,png,xpm,jpg --use-port=sdl2_mixer --use-port=sdl2_ttf")
    set_target_properties(${PROJECT_NAME} PROPERTIES LINK_FLAGS "${ECXXFLAGS} --shell-file ${SHELL_HTML_FILE} --preload-file ${ASSETS_DIRECTORY} -s FULL_ES3=1 -s ALLOW_MEMORY_GROWTH=1 -s WARN_ON_UNDEFINED_SYMBOLS=1 -s NO_EXIT_RUNTIME=1 -s AGGRESSIVE_VARIABLE_ELIMINATION=1")
    set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${ECXXFLAGS}")
    set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "index")
else()

    target_link_libraries(${PROJECT_NAME} PRIVATE ${OPENGL_LIBRARIES}
            SDL2::SDL2main
            SDL2::SDL2
            SDL2_image::SDL2_image
            SDL2_ttf::SDL2_ttf
            SDL2_mixer::SDL2_mixer)

    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/resources")
        create_resource_symlink(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_BINARY_DIR} "resources")
    endif()

endif()