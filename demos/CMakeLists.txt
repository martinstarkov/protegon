cmake_minimum_required(VERSION 3.20)

SUBDIRLIST(SUBDIRS ${CMAKE_CURRENT_SOURCE_DIR})
FOREACH(subdir ${SUBDIRS})
    add_subdirectory(${subdir})
ENDFOREACH()